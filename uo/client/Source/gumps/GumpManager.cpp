// SPDX-License-Identifier: BSD-2-Clause
#include "gumps/GumpManager.h"

#include "uo/gumps/GumpPackets.h"

#include <algorithm>

namespace uo::client::gumps
{

namespace
{

constexpr float kDragThreshold = 3.0f;      // pixels before a press becomes a drag
constexpr float kDoubleClickTime = 0.35f;   // seconds
constexpr float kTooltipDelay = 0.5f;       // seconds of hover before a tooltip shows
constexpr float kWheelStep = 20.0f;         // pixels per wheel notch in scroll areas

// Fixed-priority listeners get no hit result, so unproject the screen point through the
// default camera onto z = 0 the way PointerEvent::getWorldPoint does for hit events.
ax::Vec2 pointerWorld(ax::PointerEvent* e)
{
    if (e->hasHitResult())
    {
        return e->getWorldPoint();
    }

    auto* camera = ax::Camera::getDefaultCamera();

    if (!camera)
    {
        return e->getPoint();
    }

    ax::Ray ray = camera->screenToRay(e->getPoint());

    if (ray.direction.z == 0.0f)
    {
        return e->getPoint();
    }

    float t = -ray.origin.z / ray.direction.z;
    return ax::Vec2(ray.origin.x + t * ray.direction.x, ray.origin.y + t * ray.direction.y);
}

MouseButton toButton(int b)
{
    switch (b)
    {
    case ax::InputButton::Right:
        return MouseButton::Right;
    case ax::InputButton::Middle:
        return MouseButton::Middle;
    default:
        return MouseButton::Left;
    }
}

}  // namespace

GumpManager::GumpManager(GumpContext& ctx) : _ctx(ctx) {}

GumpManager* GumpManager::create(GumpContext& ctx)
{
    auto* m = new GumpManager(ctx);
    m->init();
    m->autorelease();
    m->setContentSize(ax::Director::getInstance()->getCanvasSize());
    m->scheduleUpdate();
    return m;
}

void GumpManager::onEnter()
{
    ax::Node::onEnter();

    _pointer = ax::PointerEventListener::create();
    _pointer->onPointerDown = [this](ax::PointerEvent* e) { return onPointerDown(e); };
    _pointer->onPointerMove = [this](ax::PointerEvent* e) { onPointerMove(e); };
    _pointer->onPointerUp = [this](ax::PointerEvent* e) { onPointerUp(e); };
    _pointer->onPointerScroll = [this](ax::PointerEvent* e) { onPointerScroll(e); };
    // Ahead of the world's scene-graph listeners: gumps sit on top of the game view.
    _eventDispatcher->addEventListenerWithFixedPriority(_pointer, -10);

    _keyboard = ax::KeyboardEventListener::create();
    _keyboard->onKeyPressed = [this](ax::KeyboardEvent* e) { onKeyPressed(e); };
    _eventDispatcher->addEventListenerWithFixedPriority(_keyboard, -10);
}

void GumpManager::onExit()
{
    _eventDispatcher->removeEventListener(_pointer);
    _eventDispatcher->removeEventListener(_keyboard);
    _pointer = nullptr;
    _keyboard = nullptr;

    for (auto* g : _gumps)
    {
        g->release();
    }

    _gumps.clear();
    ax::Node::onExit();
}

ax::Vec2 GumpManager::toScreen(const ax::Vec2& world) const
{
    ax::Vec2 local = convertToNodeSpace(world);
    return ax::Vec2(local.x, getContentSize().height - local.y);
}

ax::Vec2 GumpManager::toWorld(const ax::Vec2& screen) const
{
    return convertToWorldSpace(ax::Vec2(screen.x, getContentSize().height - screen.y));
}

void GumpManager::add(Gump* gump, const ax::Vec2& screenPos)
{
    if (!gump)
    {
        return;
    }

    gump->retain();
    _gumps.push_back(gump);
    addChild(gump);
    gump->setScreenPosition(screenPos);
    keepOnScreen(gump);
    bringToFront(gump);
}

void GumpManager::bringToFront(Gump* gump)
{
    auto it = std::find(_gumps.begin(), _gumps.end(), gump);

    if (it == _gumps.end())
    {
        return;
    }

    _gumps.erase(it);
    _gumps.push_back(gump);

    int z = 0;

    for (auto* g : _gumps)
    {
        g->setLocalZOrder(++z);
    }

    if (_heldSprite)
    {
        _heldSprite->setLocalZOrder(z + 2);
    }

    if (_tooltip)
    {
        _tooltip->setLocalZOrder(z + 1);
    }
}

void GumpManager::keepOnScreen(Gump* g)
{
    // Gump.SetInScreen: a gump opened entirely off screen moves to the origin.
    ax::Rect b = g->uoBounds();
    ax::Vec2 p = g->screenPosition();
    ax::Rect onScreen(p.x + b.origin.x, p.y + b.origin.y, b.size.width, b.size.height);
    ax::Rect window(0, 0, getContentSize().width, getContentSize().height);

    if (!window.intersectsRect(onScreen))
    {
        g->setScreenPosition(ax::Vec2(0, 0));
    }
}

Gump* GumpManager::find(GumpKind kind, uint32_t serial, uint32_t serverId) const
{
    for (auto it = _gumps.rbegin(); it != _gumps.rend(); ++it)
    {
        Gump* g = *it;

        if (!g->isClosed() && g->kind() == kind && g->serial() == serial && g->serverId() == serverId)
        {
            return g;
        }
    }

    return nullptr;
}

void GumpManager::closeAll(GumpKind kind)
{
    for (auto* g : _gumps)
    {
        if (g->kind() == kind)
        {
            g->close();
        }
    }
}

bool GumpManager::handleGumpPacket(std::span<const uint8_t> packet)
{
    auto layout = uo::gumps::decodeGump(packet);

    if (!layout)
    {
        AXLOGW("gumps: could not decode gump packet 0x{:02X} ({} bytes)", packet.empty() ? 0 : packet[0],
               packet.size());
        return false;
    }

    for (const auto& unknown : layout->unknownCommands)
    {
        AXLOGW("gumps: invalid gump command \"{}\"", unknown);
    }

    return openServerGump(*layout) != nullptr;
}

ServerGump* GumpManager::openServerGump(const uo::gumps::GumpLayout& layout)
{
    if (layout.elements.empty())
    {
        return nullptr;
    }

    ax::Vec2 pos(static_cast<float>(layout.x), static_cast<float>(layout.y));

    if (auto it = _savedPositions.find(layout.gumpId); it != _savedPositions.end())
    {
        pos = it->second;

        // Same sender and type already open: rebuild it where it is (CreateGump's reuse path).
        if (auto* existing = dynamic_cast<ServerGump*>(find(GumpKind::Server, layout.sender, layout.gumpId)))
        {
            existing->rebuild(layout);
            existing->setScreenPosition(pos);
            bringToFront(existing);
            return existing;
        }
    }
    else
    {
        _savedPositions[layout.gumpId] = pos;
    }

    auto* gump = ServerGump::create(_ctx, layout);
    add(gump, pos);
    return gump;
}

void GumpManager::closeServerGump(uint32_t gumpId, uint32_t buttonId)
{
    for (auto* g : _gumps)
    {
        auto* sg = dynamic_cast<ServerGump*>(g);

        if (!sg || sg->isClosed() || sg->serverId() != gumpId)
        {
            continue;
        }

        if (buttonId != 0)
        {
            sg->onButton(buttonId);
        }

        if (sg->canMove())
        {
            savePosition(gumpId, sg->screenPosition());
        }
        else
        {
            removePosition(gumpId);
        }

        sg->close();
    }
}

void GumpManager::prune()
{
    for (auto it = _gumps.begin(); it != _gumps.end();)
    {
        Gump* g = *it;

        if (!g->isClosed())
        {
            ++it;
            continue;
        }

        // A server gump closed by a reply remembers where it was (Gump.OnButtonClick).
        if (auto* sg = dynamic_cast<ServerGump*>(g))
        {
            if (sg->canMove())
            {
                savePosition(sg->serverId(), sg->screenPosition());
            }
            else
            {
                removePosition(sg->serverId());
            }
        }

        if (_pressedGump == g)
        {
            _pressedGump = nullptr;
            _pressedControl = nullptr;
            _dragging = false;
        }

        if (_hover && _hover->gump() == g)
        {
            _hover = nullptr;
            hideTooltip();
        }

        if (_lastClickControl && _lastClickControl->gump() == g)
        {
            _lastClickControl = nullptr;
        }

        g->removeFromParent();
        g->release();
        it = _gumps.erase(it);
    }
}

Gump* GumpManager::gumpAt(const ax::Vec2& world) const
{
    for (auto it = _gumps.rbegin(); it != _gumps.rend(); ++it)
    {
        Gump* g = *it;

        if (!g->isClosed() && g->isVisible() && g->containsWorld(world))
        {
            return g;
        }
    }

    return nullptr;
}

bool GumpManager::isOverGump(const ax::Vec2& world) const
{
    return gumpAt(world) != nullptr;
}

bool GumpManager::onPointerDown(ax::PointerEvent* e)
{
    const ax::Vec2 world = pointerWorld(e);
    _pointerWorld = world;
    hideTooltip();

    Gump* g = gumpAt(world);

    if (!g)
    {
        return false;
    }

    bringToFront(g);

    _pressedGump = g;
    _pressedControl = g->controlAt(world);
    _pressedButton = toButton(e->getButton());
    _pressWorld = world;
    _gumpStart = g->screenPosition();
    _dragging = false;
    _dragMovesGump = g->canMove() && (!_pressedControl || _pressedControl->movesGump());

    if (_pressedControl)
    {
        _pressedControl->onMouseDown(_pressedButton, _pressedControl->toLocal(world));
    }

    return true;
}

void GumpManager::setHover(Control* c)
{
    if (c == _hover)
    {
        return;
    }

    if (_hover)
    {
        _hover->onHover(false);
    }

    _hover = c;
    _hoverTime = 0;
    hideTooltip();

    if (_hover)
    {
        _hover->onHover(true);
    }
}

void GumpManager::onPointerMove(ax::PointerEvent* e)
{
    const ax::Vec2 world = pointerWorld(e);
    _pointerWorld = world;

    if (_heldSprite)
    {
        _heldSprite->setPosition(convertToNodeSpace(world));
    }

    if (_pressedGump && _pressedButton == MouseButton::Left)
    {
        if (!_dragging && world.distance(_pressWorld) >= kDragThreshold)
        {
            _dragging = true;

            // An item control can turn the drag into lifting the item.
            if (_pressedControl && _pressedControl->beginDrag())
            {
                _dragMovesGump = false;
                _pressedControl->onMouseUp(_pressedButton, false);
                _pressedControl = nullptr;
            }
        }

        if (_dragging && !_dragMovesGump && _pressedControl)
        {
            _pressedControl->onMouseDrag(_pressedButton, _pressedControl->toLocal(world));
        }

        if (_dragging && _dragMovesGump)
        {
            ax::Vec2 delta = world - _pressWorld;
            _pressedGump->setScreenPosition(ax::Vec2(_gumpStart.x + delta.x, _gumpStart.y - delta.y));
        }

        return;
    }

    Gump* g = gumpAt(world);
    setHover(g ? g->controlAt(world) : nullptr);
}

void GumpManager::onPointerUp(ax::PointerEvent* e)
{
    const ax::Vec2 world = pointerWorld(e);
    const MouseButton button = toButton(e->getButton());

    if (_held && button == MouseButton::Left)
    {
        HeldItem item = *_held;
        Gump* target = gumpAt(world);
        bool taken = false;

        if (target)
        {
            ax::Vec2 local = toScreen(world) - target->screenPosition();
            taken = target->onDropItem(item, target->controlAt(world), local);
        }

        if (!taken && !target && onDropOnWorld)
        {
            onDropOnWorld(item, toScreen(world));
        }

        _pressedGump = nullptr;
        _pressedControl = nullptr;
        _dragging = false;
        return;
    }

    if (!_pressedGump)
    {
        return;
    }

    Gump* g = _pressedGump;
    ax::RefPtr<Control> c = _pressedControl;
    const bool wasDragging = _dragging;
    _pressedGump = nullptr;
    _pressedControl = nullptr;
    _dragging = false;

    if (button != _pressedButton)
    {
        return;
    }

    bool inside = c && c->isVisible() && c->containsLocal(c->toLocal(world));

    if (c)
    {
        c->onMouseUp(button, inside);
    }

    if (wasDragging)
    {
        return;
    }

    if (button == MouseButton::Right)
    {
        // Right click closes the gump (Control.CloseWithRightClick), unless it says noclose.
        if (g->canCloseWithRightClick())
        {
            g->onCloseRequested();
        }

        return;
    }

    if (!inside)
    {
        return;
    }

    if (c == _lastClickControl && _clock - _lastClickTime <= kDoubleClickTime)
    {
        _lastClickControl = nullptr;
        c->onDoubleClick(button);
        return;
    }

    _lastClickControl = c;
    _lastClickTime = _clock;
    c->onClick(button);
}

void GumpManager::onPointerScroll(ax::PointerEvent* e)
{
    const ax::Vec2 world = pointerWorld(e);
    Gump* g = gumpAt(world);

    if (!g)
    {
        return;
    }

    // Wheel up (positive y) scrolls content up.
    if (g->onWheel(g->controlAt(world), -e->getScrollY() * kWheelStep))
    {
        e->stopPropagation();
    }
}

void GumpManager::onKeyPressed(ax::KeyboardEvent* e)
{
    if (e->getKeyCode() != ax::KeyboardEvent::KeyCode::KEY_ESCAPE || _gumps.empty())
    {
        return;
    }

    if (_held)
    {
        return;
    }

    _gumps.back()->onEscape();
}

void GumpManager::setHeldItem(std::optional<HeldItem> item)
{
    _held = std::move(item);

    if (_heldSprite)
    {
        _heldSprite->removeFromParent();
        _heldSprite = nullptr;
    }

    if (!_held)
    {
        return;
    }

    auto* tex = _ctx.textures->art(_held->graphic, _held->hue,
                                   _held->hue != 0 && _ctx.textures->artIsPartialHue(_held->graphic));

    if (tex)
    {
        _heldSprite = ax::Sprite::createWithTexture(tex);
        _heldSprite->setPosition(convertToNodeSpace(_pointerWorld));
        addChild(_heldSprite, static_cast<int>(_gumps.size()) + 2);
    }
}

void GumpManager::showTooltip(const std::string& text, const ax::Vec2& world)
{
    hideTooltip();

    GumpTextStyle style;
    style.maxWidth = 300;
    ax::Node* label = _ctx.text->createLabel(text, style);

    if (!label)
    {
        return;
    }

    const ax::Size size = label->getContentSize();
    auto* bg = ax::LayerColor::create(ax::Color32(0, 0, 0, 200), size.width + 8, size.height + 8);
    label->setIgnoreAnchorPointForPosition(false);
    label->setAnchorPoint(ax::Vec2(0, 1));
    label->setPosition(ax::Vec2(4, size.height + 4));
    bg->addChild(label);

    ax::Vec2 local = convertToNodeSpace(world);
    float x = std::min(local.x + 16, getContentSize().width - bg->getContentSize().width);
    float y = std::max(0.0f, local.y - 16 - bg->getContentSize().height);
    bg->setPosition(ax::Vec2(std::max(0.0f, x), y));
    addChild(bg, static_cast<int>(_gumps.size()) + 1);
    _tooltip = bg;
}

void GumpManager::hideTooltip()
{
    if (_tooltip)
    {
        _tooltip->removeFromParent();
        _tooltip = nullptr;
    }
}

void GumpManager::update(float dt)
{
    _clock += dt;
    prune();

    for (auto* g : _gumps)
    {
        g->refresh();
    }

    if (_hover && !_tooltip && !_pressedGump)
    {
        _hoverTime += dt;

        if (_hoverTime >= kTooltipDelay)
        {
            std::string text = _hover->tooltip();

            if (text.empty() && _hover->tooltipSerial() && propertyText)
            {
                text = propertyText(_hover->tooltipSerial());
            }

            if (!text.empty())
            {
                showTooltip(text, _pointerWorld);
            }
        }
    }
}

}  // namespace uo::client::gumps
