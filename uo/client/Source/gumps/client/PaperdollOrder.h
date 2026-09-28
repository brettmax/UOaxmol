// SPDX-License-Identifier: BSD-2-Clause
// Part of the AxmolUO client. Ported from ClassicUO Game/Data/PaperdollOrder.cs (the classic
// client's equipment draw order) and MobileView.IsCovered (layers hidden by other clothing).
// Engine free, so the world renderer can use the same order for mobiles.
#pragma once

#include "uo/world/Types.h"

#include <array>
#include <cstdint>
#include <functional>

namespace uo::world
{
class World;
class Mobile;
}

namespace uo::client::gumps
{

namespace paperdoll_order
{

// Layers 0x00..0x18 scanned by the order rules.
inline constexpr int N = 0x19;

using Graphics = std::array<uint16_t, N>;
using Order = std::array<uo::world::Layer, N>;

// Tiledata AnimID of an item graphic (what the rules key on).
using AnimIdOf = std::function<uint16_t(uint16_t graphic)>;

// Per-layer equip AnimIDs of a mobile (layers OneHanded..Legs); empty layers stay 0.
Graphics graphicsFromMobile(uo::world::World& world, const uo::world::Mobile& mobile, const AnimIdOf& animIdOf);

// Back-to-front paint order: one of three base tables, then the per-graphic reorder rules.
// altTorsoTable: female or gargoyle body.
Order build(const Graphics& graphic, bool altTorsoTable);

// Drops the sentinel and Mount (and Backpack unless includeBackpack). Returns the count
// written to dest.
int filter(const Order& order, bool includeBackpack, Order& dest);

// MobileView.IsCovered: the layer's gump is hidden by another worn item.
bool isCovered(uo::world::World& world, const uo::world::Mobile& mobile, uo::world::Layer layer,
               const AnimIdOf& animIdOf);

// Female or gargoyle bodies use the alternate torso table.
bool isGargoyleBody(uint16_t graphic);

}  // namespace paperdoll_order

}  // namespace uo::client::gumps
