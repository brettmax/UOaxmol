// SPDX-License-Identifier: BSD-2-Clause
#include "gumps/Gump.h"

#include "gumps/GumpManager.h"

namespace uo::client::gumps
{

void placeTopLeft(ax::Node* child, float x, float y, float parentHeight)
{
    child->setIgnoreAnchorPointForPosition(false);
    child->setAnchorPoint(ax::Vec2(0, 1));
    child->setPosition(ax::Vec2(x, parentHeight - y));
}

// --- Control ---------------------------------------------------------------------------

void Control::appendTooltip(const std::string& line)
{
    if (_tooltip.empty())
    {
        _tooltip = line;
    }
    else
    {
        _tooltip += '\n';
        _tooltip += line;
    }
}

void Control::setUOPosition(float x, float y)
{
    _uoX = x;
    _uoY = y;
    float parentHeight = 0;

    if (auto* parent = dynamic_cast<Control*>(getParent()))
    {
        parentHeight = parent->getContentSize().height;
    }

    placeTopLeft(this, x, y, parentHeight);
}

void Control::setUOSize(float w, float h)
{
    setContentSize(ax::Size(w, h));
    onUOSizeChanged();
}

void Control::onUOSizeChanged()
{
    // Children are placed against our height; re-place them after a resize.
    for (auto* child : getChildren())
    {
        if (auto* c = dynamic_cast<Control*>(child))
        {
            c->setUOPosition(c->uoX(), c->uoY());
        }
    }
}

ax::Vec2 Control::toLocal(const ax::Vec2& world) const
{
    ax::Vec2 p = convertToNodeSpace(world);
    return ax::Vec2(p.x, getContentSize().height - p.y);
}

bool Control::containsLocal(const ax::Vec2& local) const
{
    const auto& s = getContentSize();
    return local.x >= 0 && local.y >= 0 && local.x < s.width && local.y < s.height;
}

Gump* Control::gump() const
{
    for (const ax::Node* n = getParent(); n; n = n->getParent())
    {
        if (auto* g = dynamic_cast<const Gump*>(n))
        {
            return const_cast<Gump*>(g);
        }
    }

    return nullptr;
}

// --- Gump ------------------------------------------------------------------------------

Gump::Gump(GumpContext& ctx, GumpKind kind, uint32_t serial, uint32_t serverId)
    : _ctx(ctx), _kind(kind), _serial(serial), _serverId(serverId)
{
    setIgnoreAnchorPointForPosition(true);
    setContentSize(ax::Size(0, 0));
}

GumpManager* Gump::manager() const
{
    return dynamic_cast<GumpManager*>(const_cast<ax::Node*>(getParent()));
}

void Gump::setScreenPosition(const ax::Vec2& p)
{
    _screen = p;
    float h = getParent() ? getParent()->getContentSize().height : ax::Director::getInstance()->getCanvasSize().height;
    setPosition(ax::Vec2(p.x, h - p.y));
}

void Gump::addControl(Control* control, int page)
{
    if (!control)
    {
        return;
    }

    control->setPage(page);
    addChild(control);
    // Re-place now that the parent is known (placement depends on the parent's height).
    control->setUOPosition(control->uoX(), control->uoY());
    _controls.push_back(control);
    control->setVisible(page == 0 || page == _activePage);
}

void Gump::clearControls()
{
    for (auto* c : _controls)
    {
        c->removeFromParent();
    }

    _controls.clear();
}

void Gump::setActivePage(int page)
{
    _activePage = page;
    applyPageVisibility();
}

void Gump::applyPageVisibility()
{
    for (auto* c : _controls)
    {
        c->setVisible(c->page() == 0 || c->page() == _activePage);
    }
}

ax::Rect Gump::uoBounds() const
{
    ax::Rect r;
    bool first = true;

    for (auto* c : _controls)
    {
        if (!c->isVisible())
        {
            continue;
        }

        ax::Rect cr(c->uoX(), c->uoY(), c->getContentSize().width, c->getContentSize().height);
        r = first ? cr : r.unionWithRect(cr);
        first = false;
    }

    return r;
}

Control* Gump::hitTest(Control* c, const ax::Vec2& world, bool inputOnly) const
{
    if (!c->isVisible())
    {
        return nullptr;
    }

    // Children first, topmost last-added first.
    auto& children = c->getChildren();

    for (auto it = children.rbegin(); it != children.rend(); ++it)
    {
        if (auto* child = dynamic_cast<Control*>(*it))
        {
            if (auto* hit = hitTest(child, world, inputOnly))
            {
                return hit;
            }
        }
    }

    if (inputOnly && !c->acceptsInput())
    {
        return nullptr;
    }

    return c->containsLocal(c->toLocal(world)) ? c : nullptr;
}

Control* Gump::controlAt(const ax::Vec2& world) const
{
    for (auto it = _controls.rbegin(); it != _controls.rend(); ++it)
    {
        if (auto* hit = hitTest(*it, world, true))
        {
            return hit;
        }
    }

    return nullptr;
}

bool Gump::containsWorld(const ax::Vec2& world) const
{
    for (auto it = _controls.rbegin(); it != _controls.rend(); ++it)
    {
        if (hitTest(*it, world, false))
        {
            return true;
        }
    }

    return false;
}

void Gump::close()
{
    if (_closed)
    {
        return;
    }

    _closed = true;
    setVisible(false);
}

}  // namespace uo::client::gumps
