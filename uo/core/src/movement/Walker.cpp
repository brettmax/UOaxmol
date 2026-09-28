// SPDX-License-Identifier: BSD-2-Clause
// Ported from ClassicUO (Game/Managers/WalkerManager.cs, PlayerMobile.Walk, Mobile.ProcessSteps).

#include "uo/movement/Walker.h"

#include "uo/movement/Pathfinder.h"
#include "uo/net/OutgoingPackets.h"

namespace uo::movement
{

WalkResult Walker::walk(Direction direction, bool run, const PlayerWalkState& player, const WalkOptions& options,
                        Pathfinder& pathfinder, uint64_t nowMs)
{
    WalkResult result;

    if (_walkingFailed || _lastStepRequestTime > nowMs || _stepsCount >= kMaxStepCount ||
        (options.paralysisBlocksWalk && player.paralyzed))
    {
        return result;
    }

    direction = masked(direction);
    run |= options.alwaysRun;

    if (player.speed >= CharacterSpeed::CantRun || (player.stamina <= 1 && !player.dead) ||
        (player.hidden && options.alwaysRunUnlessHidden))
    {
        run = false;
    }

    if (!_serverDirectionInitialized)
    {
        _serverDirection = masked(player.facing);
        _serverDirectionInitialized = true;
    }

    int x = player.x;
    int y = player.y;
    int8_t z = player.z;

    // The server-synced facing, not the visual one, which coalesced turns may have moved on.
    Direction oldDirection = player.hasQueuedSteps ? player.lastQueuedDirection : _serverDirection;
    oldDirection = masked(oldDirection);

    const int8_t oldZ = z;
    int walkTime = turnDelay(options.fastRotation);
    const int stepTime = timeToCompleteMovement(run, player.mounted);

    Direction newDir = direction;
    int newX = x;
    int newY = y;
    int8_t newZ = z;
    const bool canMove = pathfinder.canWalk(newDir, newX, newY, newZ);

    if (oldDirection == direction)
    {
        if (!canMove)
        {
            return result;
        }

        // Facing the way we want to go: step, unless a blocked diagonal slid sideways, in which
        // case this request only turns toward the open side.
        if (direction == newDir)
        {
            x = newX;
            y = newY;
            z = newZ;
            walkTime = stepTime;
        }
        direction = newDir;
    }
    else
    {
        // Not facing that way yet: turn. If the pathfinder slid a diagonal into the direction we
        // already face, it is a step after all.
        if (!canMove)
        {
            newDir = direction;
        }

        if (oldDirection == newDir)
        {
            x = newX;
            y = newY;
            z = newZ;
            walkTime = stepTime;
        }
        direction = newDir;
    }

    result.direction = direction;
    result.run = run;
    result.x = x;
    result.y = y;
    result.z = z;
    result.walkTime = walkTime;
    result.turnOnly = walkTime != stepTime;

    if (result.turnOnly && _unacceptedPacketsCount >= kCoalesceTurnsAtUnconfirmed)
    {
        result.kind = WalkResult::Kind::Coalesced;
        _lastStepRequestTime = nowMs + walkTime;
        return result;
    }

    StepInfo& step = _stepInfos[_stepsCount];
    step.sequence = _walkSequence;
    step.accepted = false;
    step.running = run;
    step.oldDirection = toByte(oldDirection);
    step.direction = toByte(direction);
    step.timer = nowMs;
    step.x = static_cast<uint16_t>(x);
    step.y = static_cast<uint16_t>(y);
    step.z = z;
    step.noRotation = step.oldDirection == toByte(direction) && oldZ - z >= 11;
    _stepsCount++;

    result.kind = WalkResult::Kind::Sent;
    result.startsFromIdle = !player.hasQueuedSteps;
    result.sequence = _walkSequence;
    result.fastWalkKey = _fastWalk.take();
    result.packet = net::out::walkRequest(static_cast<uint8_t>(toByte(direction) | (run ? 0x80 : 0)), _walkSequence,
                                         result.fastWalkKey);

    _serverDirection = direction;
    _walkSequence = _walkSequence == 0xFF ? 1 : static_cast<uint8_t>(_walkSequence + 1);
    _unacceptedPacketsCount++;
    _lastStepRequestTime = nowMs + walkTime;

    return result;
}

ConfirmResult Walker::confirm(uint8_t sequence)
{
    ConfirmResult result;

    if (_unacceptedPacketsCount != 0)
    {
        _unacceptedPacketsCount--;
    }

    int stepIndex = 0;
    while (stepIndex < _stepsCount && _stepInfos[stepIndex].sequence != sequence)
    {
        stepIndex++;
    }

    bool badStep = stepIndex == _stepsCount;

    if (!badStep)
    {
        if (stepIndex >= _currentWalkSequence)
        {
            _stepInfos[stepIndex].accepted = true;
            result.rangeCenter = std::make_pair(_stepInfos[stepIndex].x, _stepInfos[stepIndex].y);
        }
        else if (stepIndex == 0)
        {
            result.rangeCenter = std::make_pair(_stepInfos[0].x, _stepInfos[0].y);

            for (int i = 1; i < _stepsCount; i++)
            {
                _stepInfos[i - 1] = _stepInfos[i];
            }

            _stepsCount--;
            _currentWalkSequence--;
        }
        else
        {
            badStep = true;
        }
    }

    if (badStep)
    {
        if (!_resendPacketResync)
        {
            result.sendResync = true;
            _resendPacketResync = true;
        }

        _walkingFailed = true;
        _stepsCount = 0;
        _currentWalkSequence = 0;
    }

    result.badStep = badStep;
    return result;
}

void Walker::deny(Direction direction)
{
    reset();
    // ClassicUO resyncs to the facing from before the deny, then applies the packet's facing; the
    // next request would then be judged against a stale facing. Use the server's.
    _serverDirection = masked(direction);
    _serverDirectionInitialized = true;
}

void Walker::resetFromServer(Direction direction)
{
    deny(direction);
}

void Walker::onStepAnimated()
{
    // Past the tracked steps (the visual queue outran a deny or resync) there is nothing to retire.
    if (_currentWalkSequence >= _stepsCount)
    {
        return;
    }

    if (_stepInfos[_currentWalkSequence].accepted)
    {
        for (int i = _currentWalkSequence + 1; i < _stepsCount; i++)
        {
            _stepInfos[i - 1] = _stepInfos[i];
        }
        _stepsCount--;
    }
    else
    {
        _currentWalkSequence++;
    }
}

void Walker::reset()
{
    _unacceptedPacketsCount = 0;
    _stepsCount = 0;
    _walkSequence = 0;
    _currentWalkSequence = 0;
    _walkingFailed = false;
    _resendPacketResync = false;
    _lastStepRequestTime = 0;
}

}  // namespace uo::movement
