// SPDX-License-Identifier: BSD-2-Clause
// Ported from ClassicUO (Game/Constants.cs, Game/Data/MovementSpeed.cs).

#pragma once

#include <cstdint>

namespace uo::movement
{

// Unconfirmed 0x02 walk requests the client may have in flight.
inline constexpr int kMaxStepCount = 5;
inline constexpr int kMaxFastWalkStackSize = 5;

inline constexpr int kTurnDelay     = 80;
inline constexpr int kTurnDelayFast = 45;

inline constexpr int kStepDelayMountRun  = 100;
inline constexpr int kStepDelayMountWalk = 200;
inline constexpr int kStepDelayRun       = 200;
inline constexpr int kStepDelayWalk      = 400;

inline constexpr int kDefaultCharacterHeight = 16;
inline constexpr int kDefaultBlockHeight     = 16;

// A direction-only request is folded into the visual facing, without a packet, once this many
// requests are unconfirmed (adaptive coalescing: full-speed spinning on a healthy connection).
inline constexpr int kCoalesceTurnsAtUnconfirmed = 3;

// Mouse distance from the player, in screen pixels, at which right-click movement runs.
inline constexpr int kMouseRunDistance = 190;

// A* node budget per search.
inline constexpr int kPathfinderMaxNodes = 10000;

enum class CharacterSpeed : uint8_t
{
    Normal,
    FastUnmount,
    CantRun,
    FastUnmountAndCantRun,
};

constexpr int turnDelay(bool fastRotation) { return fastRotation ? kTurnDelayFast : kTurnDelay; }

constexpr int timeToCompleteMovement(bool run, bool mounted)
{
    if (mounted)
    {
        return run ? kStepDelayMountRun : kStepDelayMountWalk;
    }
    return run ? kStepDelayRun : kStepDelayWalk;
}

}  // namespace uo::movement
