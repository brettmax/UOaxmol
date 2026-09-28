// SPDX-License-Identifier: BSD-2-Clause
// Part of the AxmolUO client. Ported from ClassicUO Game/UI/Gumps/ContainerGump.cs
// (BuildGump, ItemsOnAdded, OnMouseUp drop handling, minimise, corpse eye, Dispose) and
// Game/UI/Controls/ItemGump.cs (drawing, stack double draw, highlight, hit test, pick up).
#include "gumps/client/ContainerGump.h"

#include "gumps/GumpActions.h"
#include "gumps/GumpManager.h"
#include "gumps/client/ContainerItemSupport.h"

#include "uo/world/World.h"

#include <algorithm>
#include <functional>

namespace uo::client::gumps
{

using uo::world::Item;
using uo::world::Layer;

namespace
{

// Constants.BAD_CONTAINER_LAYERS, indexed by the tiledata layer.
constexpr bool kBadContainerLayers[30] = {
    false,                                           // invalid [body]
    true,  true,  true,  true,  true,  true,  true,  true,  //
    true,  true,  false, true,  true,  true,  false, false, //
    true,  true,  true,  true,                              //
    false,                                           // backpack
    true,  true,  true,  false, false, false, false, false,
};

constexpr uint16_t kHighlightHue = 0x0035;
constexpr uint16_t kCorpseEyeGump = 0x0045;
constexpr uint16_t kDropRefusedSound = 0x0051;
constexpr int kDragItemsDistance = 3;
constexpr int kChessboardOffsetY = 20;

// The container picture: double click restores a minimised container.
class ContainerBackground : public GumpPic
{
public:
    ContainerBackground(GumpContext& ctx, uint16_t graphic, std::function<void()> onDouble)
        : GumpPic(ctx, graphic), _onDouble(std::move(onDouble))
    {
    }

    void onDoubleClick(MouseButton button) override
    {
        if (button == MouseButton::Left && _onDouble)
        {
            _onDouble();
        }
    }

private:
    std::function<void()> _onDouble;
};

// HitBox: an invisible rectangle that reacts to a left click.
class ClickArea : public Control
{
public:
    ClickArea(float width, float height, std::function<void()> onLeftClick) : _onLeftClick(std::move(onLeftClick))
    {
        init();
        autorelease();
        setAcceptsInput(true);
        setUOSize(width, height);
    }

    void onClick(MouseButton button) override
    {
        if (button == MouseButton::Left && _onLeftClick)
        {
            _onLeftClick();
        }
    }

private:
    std::function<void()> _onLeftClick;
};

Control* makeLayer()
{
    auto* layer = new Control();
    layer->init();
    layer->autorelease();
    return layer;
}

ax::Sprite* topLeftSprite(ax::Texture2D* tex)
{
    if (!tex)
    {
        return nullptr;
    }

    auto* s = ax::Sprite::createWithTexture(tex);
    s->setAnchorPoint(ax::Vec2(0, 1));
    return s;
}

bool isCoinGraphic(uint16_t g)
{
    return g >= 0x0EEA && g <= 0x0EF2;
}

}  // namespace

// --- ContainerItemPic ------------------------------------------------------------------

ContainerItemPic::ContainerItemPic(ContainerGump& owner) : _owner(owner)
{
    init();
    autorelease();
    setAcceptsInput(true);
    setMovesGump(false);
    setVisible(false);
}

void ContainerItemPic::assign(uint32_t serial, uint16_t graphic, uint16_t hue, bool stacked)
{
    if (serial != _serial)
    {
        cancelSingleClick(this);
    }

    _serial = serial;
    _graphic = graphic;
    _hue = hue;
    _stacked = stacked;
    _hovered = false;
    setTooltipSerial(serial);
    setAcceptsInput(true);
    setVisible(true);
    rebuildSprites();
}

void ContainerItemPic::clear()
{
    cancelSingleClick(this);
    _serial = 0;
    _hovered = false;
    setTooltipSerial(0);
    setAcceptsInput(false);
    setVisible(false);
    removeAllChildren();
}

void ContainerItemPic::rebuildSprites()
{
    removeAllChildren();

    GumpContext& ctx = _owner.context();
    const bool isGump = _owner.itemsAreGumps();
    uint16_t hue = _hue & 0x3FFF;
    bool partial = !isGump && hue != 0 && ctx.textures->artIsPartialHue(_graphic);

    // ItemGump.AddToRenderLists: hue 0x35, not partial, under the mouse.
    if (_hovered)
    {
        hue = kHighlightHue;
        partial = false;
    }

    ax::Texture2D* tex = isGump ? ctx.textures->gump(_graphic, hue, partial) : ctx.textures->art(_graphic, hue, partial);
    const ax::Size ts = tex ? tex->getContentSize() : ax::Size(0, 0);
    const float extra = _stacked ? 5.0f : 0.0f;
    setUOSize(ts.width + extra, ts.height + extra);

    const float h = getContentSize().height;

    if (auto* s = topLeftSprite(tex))
    {
        s->setPosition(ax::Vec2(0, h));
        addChild(s);
    }

    // A stack is drawn a second time 5 pixels down and right.
    if (_stacked)
    {
        if (auto* s = topLeftSprite(tex))
        {
            s->setPosition(ax::Vec2(5, h - 5));
            addChild(s);
        }
    }
}

bool ContainerItemPic::opaqueAt(int x, int y) const
{
    if (x < 0 || y < 0)
    {
        return false;
    }

    GumpContext& ctx = _owner.context();
    return _owner.itemsAreGumps() ? ctx.textures->gumpOpaqueAt(_graphic, x, y)
                                  : ctx.textures->artOpaqueAt(_graphic, x, y);
}

bool ContainerItemPic::containsLocal(const ax::Vec2& local) const
{
    if (_serial == 0 || !Control::containsLocal(local))
    {
        return false;
    }

    const int x = static_cast<int>(local.x);
    const int y = static_cast<int>(local.y);
    return opaqueAt(x, y) || (_stacked && opaqueAt(x - 5, y - 5));
}

void ContainerItemPic::onClick(MouseButton button)
{
    if (button != MouseButton::Left || _serial == 0)
    {
        return;
    }

    if (targetIfTargeting(_owner.world(), _serial))
    {
        return;
    }

    scheduleSingleClick(this, _owner.context(), _serial);
}

void ContainerItemPic::onDoubleClick(MouseButton button)
{
    if (button != MouseButton::Left || _serial == 0)
    {
        return;
    }

    cancelSingleClick(this);

    if (_owner.world().target.isTargeting)
    {
        return;
    }

    // DoubleClickToLootInsideContainers (a profile option, off by default) is not ported.
    if (auto* actions = _owner.context().actions)
    {
        actions->doubleClick(_serial);
    }
}

void ContainerItemPic::onHover(bool entered)
{
    if (_serial == 0 || _hovered == entered)
    {
        return;
    }

    _hovered = entered;
    rebuildSprites();
}

bool ContainerItemPic::beginDrag()
{
    if (_serial == 0)
    {
        return false;
    }

    cancelSingleClick(this);
    return liftItem(_owner.context(), _owner.world(), &_owner, _serial);
}

// --- ContainerGump ---------------------------------------------------------------------

ContainerGump::ContainerGump(GumpContext& ctx, uo::world::World& world, uint32_t serial, uint16_t gumpGraphic,
                             bool playOpenSound)
    : Gump(ctx, GumpKind::Container, serial, 0), _world(world), _graphic(gumpGraphic),
      _data(containerData(gumpGraphic))
{
    init();
    autorelease();
    setCanMove(true);
    setCanCloseWithRightClick(true);
    build();
    syncItems();

    if (playOpenSound && _data.openSound != 0)
    {
        if (auto& play = itemGumpHooks().playSound)
        {
            play(_data.openSound);
        }
    }
}

void ContainerGump::build()
{
    _background = add(new ContainerBackground(context(), _graphic, [this]() {
        if (_minimized)
        {
            setMinimized(false);
        }
    }));

    if (_graphic == kCorpseGump)
    {
        _eye = add(new GumpPic(context(), kCorpseEyeGump));
        _eye->setUOPosition(45, 30);
    }

    _itemsLayer = add(makeLayer());

    if (_data.hasMinimizer())
    {
        _minimizer = add(new ClickArea(static_cast<float>(_data.minimizerWidth),
                                       static_cast<float>(_data.minimizerHeight), [this]() {
                                           GumpManager* manager = gumpManagerOf(this);

                                           if (!_minimized && !(manager && manager->heldItem()))
                                           {
                                               setMinimized(true);
                                           }
                                       }));
        _minimizer->setUOPosition(static_cast<float>(_data.minimizerX), static_cast<float>(_data.minimizerY));
    }
}

void ContainerGump::setMinimized(bool minimized)
{
    if (minimized && _data.iconizedGraphic == 0)
    {
        return;
    }

    _minimized = minimized;
    _background->setGraphic(minimized ? _data.iconizedGraphic : _graphic);

    for (auto* c : controls())
    {
        if (c != _background)
        {
            c->setVisible(!minimized);
        }
    }
}

void ContainerGump::refresh()
{
    if (isClosed())
    {
        return;
    }

    if (!_world.item(serial()))
    {
        close();
        return;
    }

    // The corpse gump's eye blinks every 750 ms.
    if (_eye)
    {
        _eyeClock += ax::Director::getInstance()->getDeltaTime();

        if (_eyeClock >= 0.75f)
        {
            _eyeClock = 0;
            _eyeFrame ^= 1;
            _eye->setGraphic(static_cast<uint16_t>(kCorpseEyeGump + _eyeFrame));
        }
    }

    syncItems();
}

void ContainerGump::collectItems(std::vector<Shown>& out)
{
    out.clear();
    Item* self = _world.item(serial());

    if (!self)
    {
        return;
    }

    uint32_t heldSerial = 0;

    if (GumpManager* manager = gumpManagerOf(this); manager && manager->heldItem())
    {
        heldSerial = manager->heldItem()->serial;
    }

    const bool isCorpse = self->graphic == uo::world::kCorpseGraphic;

    for (uint32_t childSerial : self->contents())
    {
        const Item* item = _world.item(childSerial);

        // The lifted item leaves the container until the server puts it somewhere
        // (World.ObjectToRemove in ClassicUO).
        if (!item || item->serial == heldSerial || item->amount == 0)
        {
            continue;
        }

        // ItemsOnAdded filters on the tiledata layer, not the server's.
        const ItemTileInfo info = itemTileInfo(context(), item->graphic);
        const uint8_t layer = info.layer;

        if (isCorpse && item->layer != Layer::Invalid && (layer >= 30 || !kBadContainerLayers[layer]))
        {
            continue;
        }

        if (info.wearable && (layer == static_cast<uint8_t>(Layer::Face) || layer == static_cast<uint8_t>(Layer::Beard) ||
                              layer == static_cast<uint8_t>(Layer::Hair)))
        {
            continue;
        }

        Shown s;
        s.serial = item->serial;
        s.graphic = displayedGraphic(*item);

        if (itemsAreGumps())
        {
            s.graphic = static_cast<uint16_t>(s.graphic - kItemGumpTextureOffset);
        }

        s.hue = item->hue;
        s.x = static_cast<int16_t>(item->x);
        s.y = static_cast<int16_t>(item->y) - (isChessboard() ? kChessboardOffsetY : 0);
        s.stacked = !item->isMulti && !item->isCoin() && item->amount > 1 && info.stackable;
        out.push_back(s);
    }
}

void ContainerGump::syncItems()
{
    collectItems(_scratch);

    if (_shownValid && _scratch == _shown)
    {
        return;
    }

    while (_itemPics.size() < _scratch.size())
    {
        auto* pic = new ContainerItemPic(*this);
        _itemsLayer->addChild(pic);
        _itemPics.push_back(pic);
    }

    for (size_t i = 0; i < _itemPics.size(); ++i)
    {
        ContainerItemPic* pic = _itemPics[i];

        if (i >= _scratch.size())
        {
            if (pic->serial() != 0)
            {
                pic->clear();
            }

            continue;
        }

        const Shown& s = _scratch[i];

        if (!_shownValid || i >= _shown.size() || !(_shown[i] == s) || pic->serial() != s.serial)
        {
            pic->assign(s.serial, s.graphic, s.hue, s.stacked);
        }

        pic->setUOPosition(static_cast<float>(s.x), static_cast<float>(s.y));
    }

    _shown.swap(_scratch);
    _shownValid = true;
}

uint16_t ContainerGump::heldBaseGraphic(const HeldItem& item) const
{
    uint16_t g = item.graphic;

    if (isCoinGraphic(g))
    {
        g = static_cast<uint16_t>(g - ((g - 0x0EEA) % 3));
    }

    return g;
}

bool ContainerGump::onDropItem(const HeldItem& held, Control* target, const ax::Vec2& local)
{
    GumpContext& ctx = context();
    GumpManager* manager = gumpManagerOf(this);

    if (!ctx.actions || !manager)
    {
        return false;
    }

    Item* self = _world.item(serial());

    if (!self)
    {
        return true;
    }

    const uint32_t root = _world.rootContainer(*self);

    if (root == 0 || !_world.get(root))
    {
        return true;
    }

    int x = static_cast<int>(local.x);
    int y = static_cast<int>(local.y);
    uint32_t dropContainer = serial();

    uint32_t targetSerial = 0;

    if (auto* pic = dynamic_cast<ContainerItemPic*>(target); pic && pic->isVisible())
    {
        targetSerial = pic->serial();
    }

    Item* targetItem = uo::world::isValidSerial(targetSerial) ? _world.item(targetSerial) : nullptr;
    bool canDrop = distanceToPlayer(_world, root) <= kDragItemsDistance;

    if (canDrop && targetItem)
    {
        const ItemTileInfo info = itemTileInfo(ctx, targetItem->graphic);

        if (info.container)
        {
            // Into the container under the pointer, at a server-chosen spot.
            dropContainer = targetItem->serial;
            x = 0xFFFF;
            y = 0xFFFF;
        }
        else if (info.stackable && targetItem->graphic == heldBaseGraphic(held))
        {
            // Onto a matching stack.
            dropContainer = targetItem->serial;
            x = targetItem->x;
            y = targetItem->y;
        }
        else
        {
            switch (targetItem->graphic)
            {
                case 0x0EFA:
                case 0x2253:
                case 0x2252:
                case 0x238C:
                case 0x23A0:
                case 0x2D50:
                    // Spellbooks take scrolls dropped on them.
                    dropContainer = targetItem->serial;
                    x = targetItem->x;
                    y = targetItem->y;
                    break;
                default:
                    break;
            }
        }
    }

    if (!canDrop)
    {
        if (auto& play = itemGumpHooks().playSound)
        {
            play(kDropRefusedSound);
        }

        return true;
    }

    ContainerGump* dest = manager->find<ContainerGump>(dropContainer);

    if (dest && (!targetItem || (targetItem->serial != dropContainer && !itemTileInfo(ctx, targetItem->graphic).container)))
    {
        // Centre the item on the pointer and keep it inside the container's item area.
        if (dest->isChessboard())
        {
            y += kChessboardOffsetY;
        }

        const ContainerData& d = dest->data();
        const int right = d.right;
        const int bottom = d.bottom + (dest->isChessboard() ? kChessboardOffsetY : 0);

        ax::Texture2D* tex =
            dest->itemsAreGumps()
                ? ctx.textures->gump(static_cast<uint16_t>(held.graphic - kItemGumpTextureOffset))
                : ctx.textures->art(held.graphic);

        if (tex)
        {
            const int w = static_cast<int>(tex->getContentSize().width);
            const int h = static_cast<int>(tex->getContentSize().height);

            x -= w >> 1;
            y -= h >> 1;

            if (x + w > right)
            {
                x = right - w;
            }

            if (y + h > bottom)
            {
                y = bottom - h;
            }
        }

        x = std::max(x, d.left);
        y = std::max(y, d.top);
    }

    // GameActions.DropItem: an item is never dropped into itself unless it stacks.
    if (held.serial == dropContainer && !itemTileInfo(ctx, heldBaseGraphic(held)).stackable)
    {
        return true;
    }

    ctx.actions->dropToContainer(held.serial, dropContainer, x, y);
    manager->setHeldItem(std::nullopt);
    return true;
}

void ContainerGump::onCloseRequested()
{
    if (_data.closedSound != 0)
    {
        if (auto& play = itemGumpHooks().playSound)
        {
            play(_data.closedSound);
        }
    }

    closeWithChildren();
}

void ContainerGump::closeWithChildren()
{
    if (isClosed())
    {
        return;
    }

    GumpManager* manager = gumpManagerOf(this);
    const Item* self = _world.item(serial());

    if (manager && self)
    {
        for (uint32_t child : self->contents())
        {
            if (auto* g = manager->find<ContainerGump>(child))
            {
                g->closeWithChildren();
            }
        }
    }

    close();
}

}  // namespace uo::client::gumps
