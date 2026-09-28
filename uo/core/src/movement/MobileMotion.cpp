// SPDX-License-Identifier: BSD-2-Clause
// Ported from ClassicUO (Mobile.ProcessSteps, MovementSpeed.GetPixelOffset).

#include "uo/movement/MobileMotion.h"

#include "uo/movement/Direction.h"
#include "uo/movement/MovementConstants.h"
#include "uo/movement/Walker.h"

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

MotionUpdate advanceMotion(MobileMotion& m, Walker* playerWalker, bool mounted, uint64_t nowMs, int frameDelayMs)
{
    MotionUpdate update;

    while (!m.steps.empty())
    {
        const MobileStep step = m.steps.front();

        const int delay = static_cast<int>(static_cast<int64_t>(nowMs) - static_cast<int64_t>(m.lastStepTime));
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

            if (m.offsetX == 0 && m.offsetY == 0)
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
                m.offsetZ = static_cast<int8_t>((step.z - m.z) * x * (4.0f / frames));
                pixelOffset(step.direction, x, y, frames);
                m.offsetX = static_cast<int8_t>(x);
                m.offsetY = static_cast<int8_t>(y);
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

        m.x = step.x;
        m.y = step.y;
        m.z = step.z;
        m.direction = step.direction;
        m.running = step.run;
        m.offsetX = m.offsetY = m.offsetZ = 0;
        m.steps.pop_front();
        update.stepsCompleted++;

        if (directionChange)
        {
            // A turn costs no time: carry straight on to the next step this frame.
            continue;
        }

        update.tileChanged = true;
        m.lastStepTime = nowMs;
        break;
    }

    return update;
}

}  // namespace uo::movement
