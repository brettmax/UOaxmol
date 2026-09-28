// SPDX-License-Identifier: BSD-2-Clause
// Ported from ClassicUO (GameSceneInputHandler.MoveCharacterByMouseInput, the arrow-key flags, and
// GameScene.Update's movement block). Engine-free: uo/client/input feeds it Axmol events.

#pragma once

#include "uo/movement/Direction.h"

#include <array>
#include <optional>

namespace uo::movement
{

enum class ArrowKey : uint8_t
{
    Up,
    Left,
    Down,
    Right,
};

struct MovementInputOptions
{
    bool alwaysRun{false};
    bool disableArrowKeys{false};
    bool disableAutoMove{false};  // left click while right-walking does not lock the walk on
};

// What the player's input asks for this frame.
struct MovementIntent
{
    Direction direction{Direction::North};
    bool run{false};
    bool fromMouse{false};
    bool cancelAutoWalk{false};  // mouse movement overrides a running auto-walk
};

class MovementInput
{
public:
    // Right button pressed over the game world (not over a gump).
    void rightMouseDown();
    void rightMouseUp();

    // Left button pressed over the world: while right-walking, this locks the walk on until the
    // next right press, so the player keeps moving toward the cursor.
    void leftMouseDown(const MovementInputOptions& options);

    void setArrow(ArrowKey key, bool down) { _arrows[static_cast<size_t>(key)] = down; }
    void releaseAll();

    // The movement the input asks for, if any. (centerX, centerY) is where the player is drawn and
    // (mouseX, mouseY) the cursor, both in window pixels with y down. Arrow keys are ignored while
    // auto-walking; the mouse is not, and cancels it.
    std::optional<MovementIntent> intent(int centerX, int centerY, int mouseX, int mouseY, bool autoWalking,
                                         const MovementInputOptions& options) const;

    bool mouseWalking() const { return _rightMousePressed || _continueRunning; }

private:
    std::array<bool, 4> _arrows{};
    bool _rightMousePressed{false};
    bool _continueRunning{false};
};

}  // namespace uo::movement
