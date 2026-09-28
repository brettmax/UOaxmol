// SPDX-License-Identifier: BSD-2-Clause
// Ported from ClassicUO (Mobile.ProcessSteps, MovementSpeed.GetPixelOffset): the per-frame advance
// of a mobile along its queued steps, with the pixel offset to draw it at between tiles.

#pragma once

#include <cstdint>
#include <deque>

namespace uo::movement
{

class Walker;
}  // namespace uo::movement

namespace uo::world
{
class Mobile;
}  // namespace uo::world

namespace uo::movement
{

inline constexpr int kCharacterAnimationDelay = 80;

struct MobileStep
{
    int x{0}, y{0};
    int8_t z{0};
    uint8_t direction{0};
    bool run{false};
};

// Per-mobile draw offset and step clock, kept beside a world::Mobile (which holds the tile
// position and step queue) by whoever renders it.
struct MotionClock
{
    // Screen offset from the tile position, in pixels.
    int8_t offsetX{0}, offsetY{0}, offsetZ{0};
    uint64_t lastStepTime{0};
};

struct MobileMotion : MotionClock
{
    int x{0}, y{0};
    int8_t z{0};
    uint8_t direction{0};
    bool running{false};

    std::deque<MobileStep> steps;

    // Where the mobile will be once every queued step finishes (Mobile.GetEndPosition).
    MobileStep endPosition() const
    {
        if (steps.empty())
        {
            return {x, y, z, direction, running};
        }
        return steps.back();
    }

    void clearSteps()
    {
        steps.clear();
        offsetX = offsetY = offsetZ = 0;
    }
};

struct MotionUpdate
{
    int stepsCompleted{0};
    bool tileChanged{false};  // re-sort the mobile into its new tile and refresh ranged gumps
    bool fell{false};         // the player dropped 22 or more z in one step ("Ouch!")
};

// Advances `m` for this frame. `playerWalker` is the player's walker for the player, null for
// everyone else; it is told about every completed step. `frameDelayMs` is the scene's frame time.
MotionUpdate advanceMotion(MobileMotion& m, Walker* playerWalker, bool mounted, uint64_t nowMs, int frameDelayMs);

// The same for a uo::world mobile, whose steps the packet handlers queue (Mobile::enqueueStep).
MotionUpdate advanceMotion(world::Mobile& m, MotionClock& clock, Walker* playerWalker, bool mounted, uint64_t nowMs,
                           int frameDelayMs);

// Pixel offset for progress `x`/`y` (both set to the same fraction of frames) along `dir`
// (MovementSpeed.GetPixelOffset).
void pixelOffset(uint8_t dir, float& x, float& y, float framesPerTile);

}  // namespace uo::movement
