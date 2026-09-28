// SPDX-License-Identifier: BSD-2-Clause
// Part of the AxmolUO client. Ported from ClassicUO Game/GameActions.cs (PickUp),
// Game/Managers/DelayedObjectClickManager.cs, Game/GameObjects/Item.cs (DisplayedGraphic) and
// Assets/ArtLoader.cs (GetRealArtBounds).
#include "gumps/client/ContainerItemSupport.h"

#include "gumps/GumpActions.h"
#include "gumps/GumpManager.h"

#include "uo/assets/TileData.h"
#include "uo/world/World.h"

#include <algorithm>
#include <cstdlib>
#include <unordered_map>

namespace uo::client::gumps
{

namespace
{

const uo::assets::TileData* g_tileData = nullptr;

constexpr std::string_view kSingleClickKey = "uo_item_single_click";

}  // namespace

void setItemTileData(const uo::assets::TileData* tileData)
{
    g_tileData = tileData;
}

ItemTileInfo itemTileInfo(GumpContext& ctx, uint16_t graphic)
{
    ItemTileInfo info;

    if (g_tileData)
    {
        if (const auto* t = g_tileData->staticTile(graphic))
        {
            info.layer = t->layer;
            info.animId = t->animId;
            info.partialHue = t->is(uo::assets::TF_PartialHue);
            info.stackable = t->is(uo::assets::TF_Generic);
            info.container = t->is(uo::assets::TF_Container);
            info.wearable = t->is(uo::assets::TF_Wearable);
        }

        return info;
    }

    if (ctx.textures)
    {
        info.animId = ctx.textures->artAnimId(graphic);
        info.partialHue = ctx.textures->artIsPartialHue(graphic);
        info.wearable = info.animId != 0;
    }

    return info;
}

ItemGumpHooks& itemGumpHooks()
{
    static ItemGumpHooks hooks;
    return hooks;
}

uint16_t displayedGraphic(const uo::world::Item& item)
{
    if (item.isCoin())
    {
        if (item.amount > 5)
        {
            return static_cast<uint16_t>(item.graphic + 2);
        }

        if (item.amount > 1)
        {
            return static_cast<uint16_t>(item.graphic + 1);
        }
    }

    return item.graphic;
}

GumpManager* gumpManagerOf(Gump* gump)
{
    return gump ? dynamic_cast<GumpManager*>(gump->getParent()) : nullptr;
}

void scheduleSingleClick(Control* owner, GumpContext& ctx, uint32_t serial)
{
    if (!owner || !ctx.actions || !uo::world::isValidSerial(serial))
    {
        return;
    }

    GumpActions* actions = ctx.actions;
    owner->unschedule(kSingleClickKey);
    owner->scheduleOnce([actions, serial](float) { actions->singleClick(serial); }, kItemDoubleClickDelay,
                        kSingleClickKey);
}

void cancelSingleClick(Control* owner)
{
    if (owner)
    {
        owner->unschedule(kSingleClickKey);
    }
}

bool targetIfTargeting(uo::world::World& world, uint32_t serial)
{
    if (!world.target.isTargeting || !uo::world::isValidSerial(serial))
    {
        return false;
    }

    auto& hooks = itemGumpHooks();

    if (!hooks.targetObject)
    {
        return false;
    }

    hooks.targetObject(serial);
    return true;
}

bool liftItem(GumpContext& ctx, uo::world::World& world, Gump* gump, uint32_t serial)
{
    GumpManager* manager = gumpManagerOf(gump);
    const uo::world::Player* player = world.player();

    if (!manager || !ctx.actions || !player || player->isDead() || manager->heldItem())
    {
        return false;
    }

    uo::world::Item* item = world.item(serial);

    if (!item || item->isMulti)
    {
        return false;
    }

    // No split menu: the whole stack is lifted (ClassicUO opens SplitMenuGump for stacks
    // unless HoldShiftToSplitStack says otherwise).
    const uint16_t amount = std::max<uint16_t>(1, item->amount);

    HeldItem held;
    held.serial = item->serial;
    held.graphic = displayedGraphic(*item);
    held.hue = item->hue;
    held.amount = amount;
    held.sourceContainer = item->container;

    ctx.actions->pickUp(item->serial, amount);
    manager->setHeldItem(held);

    if (auto& onLifted = itemGumpHooks().onLifted)
    {
        onLifted(*item, amount);
    }

    return true;
}

ax::Rect realArtBounds(GumpContext& ctx, uint16_t graphic)
{
    static std::unordered_map<uint16_t, ax::Rect> cache;

    if (auto it = cache.find(graphic); it != cache.end())
    {
        return it->second;
    }

    ax::Rect r;
    ax::Texture2D* tex = ctx.textures ? ctx.textures->art(graphic) : nullptr;

    if (tex)
    {
        const int w = static_cast<int>(tex->getContentSize().width);
        const int h = static_cast<int>(tex->getContentSize().height);
        int minX = w, minY = h, maxX = -1, maxY = -1;

        for (int y = 0; y < h; ++y)
        {
            for (int x = 0; x < w; ++x)
            {
                if (ctx.textures->artOpaqueAt(graphic, x, y))
                {
                    minX = std::min(minX, x);
                    maxX = std::max(maxX, x);
                    minY = std::min(minY, y);
                    maxY = std::max(maxY, y);
                }
            }
        }

        if (maxX >= minX && maxY >= minY)
        {
            r = ax::Rect(static_cast<float>(minX), static_cast<float>(minY), static_cast<float>(maxX - minX + 1),
                         static_cast<float>(maxY - minY + 1));
        }
        else
        {
            r = ax::Rect(0, 0, static_cast<float>(w), static_cast<float>(h));
        }
    }

    cache.emplace(graphic, r);
    return r;
}

int distanceToPlayer(uo::world::World& world, uint32_t entitySerial)
{
    const uo::world::Player* player = world.player();

    if (!player)
    {
        return 0x7FFFFFFF;
    }

    if (entitySerial == player->serial)
    {
        return 0;
    }

    const uo::world::Entity* e = world.get(entitySerial);

    if (!e)
    {
        return 0x7FFFFFFF;
    }

    return std::max(std::abs(static_cast<int>(e->x) - static_cast<int>(player->x)),
                    std::abs(static_cast<int>(e->y) - static_cast<int>(player->y)));
}

}  // namespace uo::client::gumps
