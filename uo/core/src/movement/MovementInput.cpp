// SPDX-License-Identifier: BSD-2-Clause
// Ported from ClassicUO (GameSceneInputHandler, GameScene.Update).

#include "uo/movement/MovementInput.h"

#include "uo/movement/MovementConstants.h"

#include <cmath>

namespace uo::movement
{

void MovementInput::rightMouseDown()
{
    _rightMousePressed = true;
    _continueRunning = false;
}

void MovementInput::rightMouseUp()
{
    _rightMousePressed = false;
}

void MovementInput::leftMouseDown(const MovementInputOptions& options)
{
    if (!options.disableAutoMove && _rightMousePressed)
    {
        _continueRunning = true;
    }
}

void MovementInput::releaseAll()
{
    _arrows.fill(false);
    _rightMousePressed = false;
    _continueRunning = false;
}

std::optional<MovementIntent> MovementInput::intent(int centerX, int centerY, int mouseX, int mouseY,
                                                    bool autoWalking, const MovementInputOptions& options) const
{
    if (mouseWalking())
    {
        MovementIntent out;
        out.direction = mouseWalkDirection(centerX, centerY, mouseX, mouseY);
        out.run = std::hypot(static_cast<double>(centerX - mouseX), static_cast<double>(centerY - mouseY)) >=
                  kMouseRunDistance;
        out.fromMouse = true;
        out.cancelAutoWalk = autoWalking;
        return out;
    }

    if (options.disableArrowKeys || autoWalking)
    {
        return std::nullopt;
    }

    const Direction dir = directionFromArrows(_arrows[static_cast<size_t>(ArrowKey::Up)],
                                              _arrows[static_cast<size_t>(ArrowKey::Down)],
                                              _arrows[static_cast<size_t>(ArrowKey::Left)],
                                              _arrows[static_cast<size_t>(ArrowKey::Right)]);
    if (dir == kDirectionNone)
    {
        return std::nullopt;
    }

    MovementIntent out;
    out.direction = dir;
    out.run = options.alwaysRun;
    return out;
}

}  // namespace uo::movement
