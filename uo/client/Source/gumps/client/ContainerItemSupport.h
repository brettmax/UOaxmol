// SPDX-License-Identifier: BSD-2-Clause
// Part of the AxmolUO client. Shared helpers for the container and paperdoll gumps, ported
// from the pieces of ClassicUO they both lean on: GameActions.PickUp (lift), the
// DelayedObjectClickManager (single click that waits out a double click), Item.DisplayedGraphic,
// ArtLoader.GetRealArtBounds and the tiledata lookups ItemGump / PaperDollGump make.
#pragma once

#include "gumps/Gump.h"

#include "uo/world/Entity.h"

#include <cstdint>
#include <functional>

namespace uo::assets
{
class TileData;
}

namespace uo::world
{
class World;
}

namespace uo::client::gumps
{

class GumpManager;

// The tiledata facts the item gumps need beyond GumpTextures.
struct ItemTileInfo
{
    uint8_t layer = 0;
    uint16_t animId = 0;
    bool partialHue = false;
    bool stackable = false;
    bool container = false;
    bool wearable = false;
};

// Points the lookups at the loaded tiledata (Installation::tileData()). Until it is set,
// itemTileInfo falls back to GumpTextures (anim id and partial hue only).
void setItemTileData(const uo::assets::TileData* tileData);
ItemTileInfo itemTileInfo(GumpContext& ctx, uint16_t graphic);

// Things the item gumps need from outside the gump layer. Set once at startup; every hook is
// optional.
struct ItemGumpHooks
{
    // Plays a sound effect (container open / close, drop refused).
    std::function<void(uint16_t sound)> playSound;
    // A target cursor is up and an item in a gump was clicked: send the target response.
    // Called only while world.target.isTargeting.
    std::function<void(uint32_t serial)> targetObject;
    // An item was lifted (0x07 sent). The client's drag-and-drop code records a
    // uo::world::HeldItem from `item` here so 0x27 can put it back
    // (PacketHandlers::restoreHeldItem), and may remove it from the world as ClassicUO's
    // World.ObjectToRemove does. The gumps hide the held serial either way.
    std::function<void(const uo::world::Item& item, uint16_t amount)> onLifted;
};
ItemGumpHooks& itemGumpHooks();

// Item.DisplayedGraphic: coins show a pile for amounts over 1 and 5.
uint16_t displayedGraphic(const uo::world::Item& item);

// The manager that owns a gump (its parent), or nullptr before it is added.
GumpManager* gumpManagerOf(Gump* gump);

// Mouse.MOUSE_DELAY_DOUBLE_CLICK.
inline constexpr float kItemDoubleClickDelay = 0.35f;

// DelayedObjectClickManager: a single click on an item is sent only once the double-click
// time has passed without a second click. `owner` runs the timer, so it dies with it.
void scheduleSingleClick(Control* owner, GumpContext& ctx, uint32_t serial);
void cancelSingleClick(Control* owner);

// A click on an item while a target cursor is up targets it. Returns true when it did.
bool targetIfTargeting(uo::world::World& world, uint32_t serial);

// GameActions.PickUp for an item in a gump: sends 0x07 for the whole stack and hangs the
// item on the cursor. Returns false when nothing was lifted (dead, already holding, multi).
bool liftItem(GumpContext& ctx, uo::world::World& world, Gump* gump, uint32_t serial);

// ArtLoader.GetRealArtBounds: the opaque rectangle of an item's art, top-left origin.
// Cached per graphic.
ax::Rect realArtBounds(GumpContext& ctx, uint16_t graphic);

// Player-to-entity distance used by the drop checks (Chebyshev, as GameObject.Distance).
int distanceToPlayer(uo::world::World& world, uint32_t entitySerial);

}  // namespace uo::client::gumps
