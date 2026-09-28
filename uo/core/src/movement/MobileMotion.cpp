// SPDX-License-Identifier: BSD-2-Clause
// Ported from ClassicUO (Mobile.ProcessSteps, MovementSpeed.GetPixelOffset).

#include "uo/movement/MobileMotion.h"

#include "uo/movement/Direction.h"
#include "uo/movement/MovementConstants.h"
#include "uo/movement/Walker.h"
#include "uo/world/Entity.h"

#include <cmath>
#include <cstdlib>

namespace uo::movement
{

void pixelOffset(uint8_t dir, float& x, float& y, float framesPerTile)
{
    const float stepD = 44.0f / framesPerTile;
    const float step = 22.0f / framesPerTile;
    int checkX = 22;
    int checkY = 22;

    switch (dir & 7)
    {
    case 0: x *= step;   y *= -step; break;
    case 1: x *= stepD;  checkX = 44; y = 0.0f; break;
    case 2: x *= step;   y *= step; break;
    case 3: x = 0.0f;    y *= stepD; checkY = 44; break;
    case 4: x *= -step;  y *= step; break;
    case 5: x *= -stepD; checkX = 44; y = 0.0f; break;
    case 6: x *= -step;  y *= -step; break;
    case 7: x = 0.0f;    y *= -stepD; checkY = 44; break;
    }

    const int valueX = static_cast<int>(x);
    if (std::abs(valueX) > checkX)
    {
        x = valueX < 0 ? static_cast<float>(-checkX) : static_cast<float>(checkX);
    }

    const int valueY = static_cast<int>(y);
    if (std::abs(valueY) > checkY)
    {
        y = valueY < 0 ? static_cast<float>(-checkY) : static_cast<float>(checkY);
    }
}

namespace
{

MobileStep toStep(const MobileStep& s) { return s; }

MobileStep toStep(const world::MobileStep& s)
{
    return {s.x, s.y, s.z, static_cast<uint8_t>(s.direction), s.run};
}

void place(MobileMotion& m, const MobileStep& s)
{
    m.x = s.x;
    m.y = s.y;
    m.z = s.z;
    m.direction = s.direction;
    m.running = s.run;
}

void place(world::Mobile& m, const MobileStep& s)
{
    m.x = static_cast<uint16_t>(s.x);
    m.y = static_cast<uint16_t>(s.y);
    m.z = s.z;
    m.direction = static_cast<world::Direction>(s.direction);
    m.isRunning = s.run;
}

template <class M>
MotionUpdate advance(M& m, MotionClock& c, Walker* playerWalker, bool mounted, uint64_t nowMs, int frameDelayMs)
{
    MotionUpdate update;

    while (!m.steps.empty())
    {
        const MobileStep step = toStep(m.steps.front());

        const int delay = static_cast<int>(static_cast<int64_t>(nowMs) - static_cast<int64_t>(c.lastStepTime));
        bool stepMounted = mounted;

        // Several server moves inside one mounted step interval would teleport the mobile; treat
        // them as mounted so they animate at that pace. A zero delay means one frame got them all.
        if (!stepMounted && playerWalker == nullptr && m.steps.size() > 1 && delay > 0)
        {
            stepMounted = delay <= (step.run ? kStepDelayMountRun : kStepDelayMountWalk);
        }

        const int maxDelay = timeToCompleteMovement(step.run, stepMounted) - frameDelayMs;
        bool removeStep = delay >= maxDelay;
        bool directionChange = false;

        if (m.x != step.x || m.y != step.y)
        {
            bool badStep = false;

            if (c.offsetX == 0 && c.offsetY == 0)
            {
                const int absX = std::abs(m.x - step.x);
                const int absY = std::abs(m.y - step.y);
                badStep = absX > 1 || absY > 1 || absX + absY == 0;

                if (!badStep)
                {
                    int nx = m.x;
                    int ny = m.y;
                    offsetXY(toDirection(step.direction), nx, ny);
                    badStep = nx != step.x || ny != step.y;
                }
            }

            if (badStep)
            {
                removeStep = true;
            }
            else
            {
                const float frames = maxDelay / static_cast<float>(kCharacterAnimationDelay);
                float x = delay / static_cast<float>(kCharacterAnimationDelay);
                float y = x;
                c.offsetZ = static_cast<int8_t>((step.z - m.z) * x * (4.0f / frames));
                pixelOffset(step.direction, x, y, frames);
                c.offsetX = static_cast<int8_t>(x);
                c.offsetY = static_cast<int8_t>(y);
            }
        }
        else
        {
            directionChange = true;
            removeStep = true;
        }

        if (!removeStep)
        {
            break;
        }

        if (playerWalker != nullptr)
        {
            if (m.z - step.z >= 22)
            {
                update.fell = true;
            }
            playerWalker->onStepAnimated();
        }

        place(m, step);
        c.offsetX = c.offsetY = c.offsetZ = 0;
        m.steps.pop_front();
        update.stepsCompleted++;

        if (directionChange)
        {
            // A turn costs no time: carry straight on to the next step this frame.
            continue;
        }

        update.tileChanged = true;
        c.lastStepTime = nowMs;
        break;
    }

    return update;
}

}  // namespace

MotionUpdate advanceMotion(MobileMotion& m, Walker* playerWalker, bool mounted, uint64_t nowMs, int frameDelayMs)
{
    return advance(m, m, playerWalker, mounted, nowMs, frameDelayMs);
}

MotionUpdate advanceMotion(world::Mobile& m, MotionClock& clock, Walker* playerWalker, bool mounted, uint64_t nowMs,
                           int frameDelayMs)
{
    return advance(m, clock, playerWalker, mounted, nowMs, frameDelayMs);
}

}  // namespace uo::movement
