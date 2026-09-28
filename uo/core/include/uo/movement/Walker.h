// SPDX-License-Identifier: BSD-2-Clause
// Ported from ClassicUO (Game/Managers/WalkerManager.cs, PlayerMobile.Walk, Mobile.ProcessSteps).
//
// UO movement protocol:
//   The client sends 0x02 (direction | 0x80 when running, sequence, fast-walk key) for every turn
//   and every step. The server answers 0x22 (sequence, notoriety) to accept or 0x21 (sequence, x,
//   y, direction, z) to reject and snap the player back. Up to five requests may be unconfirmed.
//   Sequences run 0..255 and wrap to 1, never back to 0. A 0x22 for an unknown sequence means the
//   client is out of step; it sends an empty 0x22 to ask the server for a resync.
//
//   Turns have no step delay beyond turnDelay(); steps take 400 ms walking, 200 ms running or
//   mounted walking, and 100 ms mounted running.

#pragma once

#include "uo/movement/Direction.h"
#include "uo/movement/FastWalkStack.h"
#include "uo/movement/MovementConstants.h"

#include <array>
#include <cstdint>
#include <optional>
#include <vector>

namespace uo::movement
{

class Pathfinder;

struct StepInfo
{
    uint8_t direction{0};
    uint8_t oldDirection{0};
    uint8_t sequence{0};
    bool accepted{false};
    bool running{false};
    bool noRotation{false};
    uint64_t timer{0};
    uint16_t x{0}, y{0};
    int8_t z{0};
};

// The player as the walk decision needs it. `x/y/z/direction` are where the player will be once
// the queued visual steps finish (Mobile.GetEndPosition); `hasQueuedSteps` says whether any are.
struct PlayerWalkState
{
    int x{0}, y{0};
    int8_t z{0};
    Direction facing{Direction::North};  // the current visual facing
    bool hasQueuedSteps{false};
    Direction lastQueuedDirection{Direction::North};

    bool paralyzed{false};
    bool dead{false};
    bool hidden{false};
    bool mounted{false};  // mounted, flying, or a fast-unmount speed mode
    CharacterSpeed speed{CharacterSpeed::Normal};
    int stamina{0};
};

struct WalkOptions
{
    bool alwaysRun{false};
    bool alwaysRunUnlessHidden{false};
    bool fastRotation{false};
    bool paralysisBlocksWalk{true};  // client 6.0.14.2 and later
};

// What a walk request did.
struct WalkResult
{
    enum class Kind : uint8_t
    {
        Rejected,   // nothing happens
        Coalesced,  // turn only: set the visual facing to `direction`, no packet
        Sent,       // `packet` must be sent and the step queued on the player
    };

    Kind kind{Kind::Rejected};
    Direction direction{Direction::North};
    bool run{false};
    bool turnOnly{false};
    bool startsFromIdle{false};  // the visual queue was empty: start the walk animation now
    int x{0}, y{0};
    int8_t z{0};
    int walkTime{0};
    uint8_t sequence{0};
    uint32_t fastWalkKey{0};
    std::vector<uint8_t> packet;  // the 0x02 request
};

struct ConfirmResult
{
    bool badStep{false};
    bool sendResync{false};  // send uo::net::out::resync() (an empty 0x22), once per failure
    std::optional<std::pair<uint16_t, uint16_t>> rangeCenter;  // tile the server has now placed the player on
};

class Walker
{
public:
    // Asks to turn or step. Mirrors PlayerMobile.Walk: rejects while a request is still timing out,
    // five are unconfirmed, walking failed, or the player is paralyzed; picks run or walk; resolves
    // the destination through the pathfinder; and folds turns into the facing without a packet
    // once the server falls behind.
    WalkResult walk(Direction direction, bool run, const PlayerWalkState& player, const WalkOptions& options,
                    Pathfinder& pathfinder, uint64_t nowMs);

    // 0x22 from the server.
    ConfirmResult confirm(uint8_t sequence);

    // 0x21 from the server. The caller clears the player's visual steps and moves the player to
    // (x, y, z) facing `direction`.
    void deny(Direction direction);

    // The server placed the player (0x20 or 0x78 for the player): forget every in-flight request.
    void resetFromServer(Direction direction);

    // A visual step of the player finished animating (Mobile.ProcessSteps removing a step).
    void onStepAnimated();

    void reset();

    FastWalkStack& fastWalk() { return _fastWalk; }
    uint64_t lastStepRequestTime() const { return _lastStepRequestTime; }
    int stepsCount() const { return _stepsCount; }
    int unacceptedCount() const { return _unacceptedPacketsCount; }
    bool walkingFailed() const { return _walkingFailed; }
    uint8_t nextSequence() const { return _walkSequence; }
    Direction serverDirection() const { return _serverDirection; }
    const StepInfo& step(int index) const { return _stepInfos[index]; }

    // Whether auto-walk may issue its next step now.
    bool canRequestStep(uint64_t nowMs) const
    {
        return _stepsCount < kMaxStepCount && _lastStepRequestTime <= nowMs;
    }

private:
    FastWalkStack _fastWalk;
    std::array<StepInfo, kMaxStepCount> _stepInfos{};
    int _stepsCount{0};
    int _currentWalkSequence{0};
    int _unacceptedPacketsCount{0};
    uint8_t _walkSequence{0};
    uint64_t _lastStepRequestTime{0};
    bool _walkingFailed{false};
    bool _resendPacketResync{false};
    Direction _serverDirection{Direction::North};
    bool _serverDirectionInitialized{false};
};

}  // namespace uo::movement
