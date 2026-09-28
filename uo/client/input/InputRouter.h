// SPDX-License-Identifier: BSD-2-Clause
// Axmol side of movement and targeting input. Ported from ClassicUO's GameSceneInputHandler (mouse
// buttons, arrow keys, Escape) and Input/Mouse (double-click timing). The decisions live in uocore
// (uo::movement::MovementInput, uo::game::TargetCursor); this class only turns Axmol pointer and
// keyboard events into calls on them and into callbacks for the game scene.

#pragma once

#include "uo/movement/MovementInput.h"

#include "axmol/base/KeyboardEvent.h"
#include "axmol/math/Vec2.h"

#include <cstdint>
#include <functional>

namespace ax
{
class Node;
class PointerEvent;
class KeyboardEventListener;
class PointerEventListener;
}  // namespace ax

namespace uo::client::input
{

enum class MouseButton : uint8_t
{
    Left,
    Right,
    Middle,
};

class InputRouter
{
public:
    // Everything positional is in window pixels, origin top-left, y down (PointerEvent::getPoint).
    struct Callbacks
    {
        // A button went down over the game world rather than over a gump. Gumps registered with
        // scene-graph priority claim their own clicks first, so this is only asked for the rest.
        std::function<bool(ax::Vec2)> isOverWorld;

        std::function<void(MouseButton, ax::Vec2)> onClick;
        std::function<void(MouseButton, ax::Vec2)> onDoubleClick;  // right: pathfind to the tile

        // Escape: the scene cancels the target cursor if one is up, else a cancellable auto-walk.
        std::function<void()> onEscape;

        // Arrow keys only walk while the chat line is empty.
        std::function<bool()> chatLineEmpty;

        // Every key the router does not use itself, for macros and chat (key, modifiers, down).
        std::function<void(ax::KeyboardEvent::KeyCode, uint32_t, bool)> onKey;
    };

    static constexpr uint64_t kDoubleClickMs = 350;  // Mouse.MOUSE_DELAY_DOUBLE_CLICK

    InputRouter(Callbacks callbacks, uo::movement::MovementInputOptions options);
    ~InputRouter();

    InputRouter(const InputRouter&) = delete;
    InputRouter& operator=(const InputRouter&) = delete;

    // Registers the listeners on `owner`'s event dispatcher at a positive fixed priority, so gumps
    // and other scene-graph listeners see events first. Detaches from any earlier owner.
    void attach(ax::Node* owner);
    void detach();

    // Forget held buttons and keys, e.g. when the window loses focus.
    void releaseAll();

    // Movement for this frame; `playerScreen` is where the player is drawn, in the same pixels.
    std::optional<uo::movement::MovementIntent> movementIntent(ax::Vec2 playerScreen, bool autoWalking) const;

    ax::Vec2 mousePosition() const { return _mouse; }
    uint32_t modifiers() const { return _modifiers; }
    bool shift() const { return (_modifiers & ax::KeyboardEvent::SHIFT) != 0; }
    bool ctrl() const { return (_modifiers & ax::KeyboardEvent::CONTROL) != 0; }
    bool alt() const { return (_modifiers & ax::KeyboardEvent::ALT) != 0; }

    // The target cursor consumed a click; a second click must not become a double-click.
    void cancelDoubleClick() { _cancelDoubleClick = true; }

    void setOptions(const uo::movement::MovementInputOptions& options) { _options = options; }
    const uo::movement::MovementInput& movement() const { return _movement; }

    // Clock source in milliseconds; defaults to std::chrono::steady_clock.
    std::function<uint64_t()> now;

private:
    bool pointerDown(ax::PointerEvent* e);
    void pointerUp(ax::PointerEvent* e);
    void pointerMove(ax::PointerEvent* e);
    void keyDown(ax::KeyboardEvent* e);
    void keyUp(ax::KeyboardEvent* e);

    struct ButtonState
    {
        uint64_t lastClick{0};
        ax::Vec2 lastPosition;
        bool down{false};
    };

    Callbacks _callbacks;
    uo::movement::MovementInputOptions _options;
    uo::movement::MovementInput _movement;
    ButtonState _buttons[3];
    ax::Vec2 _mouse;
    uint32_t _modifiers{0};
    bool _cancelDoubleClick{false};

    ax::Node* _owner{nullptr};
    ax::PointerEventListener* _pointer{nullptr};
    ax::KeyboardEventListener* _keyboard{nullptr};
};

}  // namespace uo::client::input
