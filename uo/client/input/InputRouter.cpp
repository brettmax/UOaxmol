// SPDX-License-Identifier: BSD-2-Clause
// Ported from ClassicUO (GameSceneInputHandler: OnRightMouseDown/Up, OnLeftMouseDown, OnKeyDown/Up
// arrow flags and Escape; Input/Mouse double-click timing).

#include "InputRouter.h"

#include "axmol/base/EventDispatcher.h"
#include "axmol/base/KeyboardEventListener.h"
#include "axmol/base/PointerEvent.h"
#include "axmol/base/PointerEventListener.h"
#include "axmol/scene/Node.h"

#include <chrono>

namespace uo::client::input
{

using ax::KeyboardEvent;
using KeyCode = ax::KeyboardEvent::KeyCode;
using uo::movement::ArrowKey;

namespace
{

// After gumps (scene-graph priority, which dispatches before positive fixed priorities).
constexpr int kWorldInputPriority = 10;

std::optional<MouseButton> toButton(int button)
{
    switch (button)
    {
    case ax::InputButton::Left: return MouseButton::Left;
    case ax::InputButton::Right: return MouseButton::Right;
    case ax::InputButton::Middle: return MouseButton::Middle;
    default: return std::nullopt;
    }
}

std::optional<ArrowKey> toArrow(KeyCode key)
{
    switch (key)
    {
    case KeyCode::KEY_UP_ARROW: return ArrowKey::Up;
    case KeyCode::KEY_LEFT_ARROW: return ArrowKey::Left;
    case KeyCode::KEY_DOWN_ARROW: return ArrowKey::Down;
    case KeyCode::KEY_RIGHT_ARROW: return ArrowKey::Right;
    default: return std::nullopt;
    }
}

uint64_t steadyMs()
{
    using namespace std::chrono;
    return static_cast<uint64_t>(duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count());
}

}  // namespace

InputRouter::InputRouter(Callbacks callbacks, uo::movement::MovementInputOptions options)
    : now(steadyMs), _callbacks(std::move(callbacks)), _options(options)
{
}

InputRouter::~InputRouter()
{
    detach();
}

void InputRouter::attach(ax::Node* owner)
{
    detach();
    if (owner == nullptr)
    {
        return;
    }

    _owner = owner;
    auto* dispatcher = owner->getEventDispatcher();

    _pointer = ax::PointerEventListener::create();
    _pointer->onPointerDown = [this](ax::PointerEvent* e) { return pointerDown(e); };
    _pointer->onPointerUp = [this](ax::PointerEvent* e) { pointerUp(e); };
    _pointer->onPointerMove = [this](ax::PointerEvent* e) { pointerMove(e); };
    _pointer->onPointerCancel = [this](ax::PointerEvent* e) { pointerUp(e); };
    dispatcher->addEventListenerWithFixedPriority(_pointer, kWorldInputPriority);

    _keyboard = ax::KeyboardEventListener::create();
    _keyboard->onKeyPressed = [this](KeyboardEvent* e) { keyDown(e); };
    _keyboard->onKeyReleased = [this](KeyboardEvent* e) { keyUp(e); };
    dispatcher->addEventListenerWithFixedPriority(_keyboard, kWorldInputPriority);
}

void InputRouter::detach()
{
    if (_owner == nullptr)
    {
        return;
    }

    auto* dispatcher = _owner->getEventDispatcher();
    dispatcher->removeEventListener(_pointer);
    dispatcher->removeEventListener(_keyboard);
    _pointer = nullptr;
    _keyboard = nullptr;
    _owner = nullptr;
    releaseAll();
}

void InputRouter::releaseAll()
{
    _movement.releaseAll();
    for (auto& b : _buttons)
    {
        b.down = false;
    }
    _modifiers = 0;
}

std::optional<uo::movement::MovementIntent> InputRouter::movementIntent(ax::Vec2 playerScreen, bool autoWalking) const
{
    return _movement.intent(static_cast<int>(playerScreen.x), static_cast<int>(playerScreen.y),
                            static_cast<int>(_mouse.x), static_cast<int>(_mouse.y), autoWalking, _options);
}

bool InputRouter::pointerDown(ax::PointerEvent* e)
{
    _mouse = e->getPoint();

    const auto button = toButton(e->getButton());
    if (!button)
    {
        return false;
    }

    // Anything that reached us and is not over the world (a gump without its own listener, the
    // chat bar) is not ours to handle.
    if (_callbacks.isOverWorld && !_callbacks.isOverWorld(_mouse))
    {
        return false;
    }

    ButtonState& state = _buttons[static_cast<int>(*button)];
    state.down = true;

    switch (*button)
    {
    case MouseButton::Left: _movement.leftMouseDown(_options); break;
    case MouseButton::Right: _movement.rightMouseDown(); break;
    case MouseButton::Middle: break;
    }

    const uint64_t t = now();
    const bool isDouble = !_cancelDoubleClick && state.lastClick != 0 && t - state.lastClick < kDoubleClickMs;
    _cancelDoubleClick = false;

    if (isDouble)
    {
        state.lastClick = 0;
        if (_callbacks.onDoubleClick)
        {
            _callbacks.onDoubleClick(*button, _mouse);
        }
    }
    else
    {
        state.lastClick = t;
        state.lastPosition = _mouse;
    }

    // Claim the press so the release comes back here even if it happens over a gump.
    return true;
}

void InputRouter::pointerUp(ax::PointerEvent* e)
{
    _mouse = e->getPoint();

    const auto button = toButton(e->getButton());
    if (!button)
    {
        return;
    }

    ButtonState& state = _buttons[static_cast<int>(*button)];
    if (!state.down)
    {
        return;
    }
    state.down = false;

    if (*button == MouseButton::Right)
    {
        _movement.rightMouseUp();
    }

    // A release that is not the second half of a double-click is a click.
    if (state.lastClick != 0 && _callbacks.onClick)
    {
        _callbacks.onClick(*button, _mouse);
    }
}

void InputRouter::pointerMove(ax::PointerEvent* e)
{
    _mouse = e->getPoint();
}

void InputRouter::keyDown(KeyboardEvent* e)
{
    _modifiers = e->getModifiers();
    const KeyCode key = e->getKeyCode();

    if (key == KeyCode::KEY_ESCAPE)
    {
        if (_callbacks.onEscape)
        {
            _callbacks.onEscape();
        }
        return;
    }

    if (const auto arrow = toArrow(key))
    {
        if (!_callbacks.chatLineEmpty || _callbacks.chatLineEmpty())
        {
            _movement.setArrow(*arrow, true);
        }
        return;
    }

    if (_callbacks.onKey)
    {
        _callbacks.onKey(key, _modifiers, true);
    }
}

void InputRouter::keyUp(KeyboardEvent* e)
{
    _modifiers = e->getModifiers();
    const KeyCode key = e->getKeyCode();

    // Releases always clear, so a key pressed before the chat got text cannot stick.
    if (const auto arrow = toArrow(key))
    {
        _movement.setArrow(*arrow, false);
        return;
    }

    if (key != KeyCode::KEY_ESCAPE && _callbacks.onKey)
    {
        _callbacks.onKey(key, _modifiers, false);
    }
}

}  // namespace uo::client::input
