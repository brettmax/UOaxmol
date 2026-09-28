// SPDX-License-Identifier: BSD-2-Clause
// Part of the AxmolUO client core. Ported from ClassicUO.Client: Game/Data/PaperdollOrder.cs,
// Game/Data/Mounts.cs, Game/Data/ChairTable.cs and the equipment rules of
// Game/GameObjects/Views/MobileView.cs (IsCovered, FixGargoyleEquipments).
#pragma once

#include "uo/anim/AnimTypes.h"

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

namespace uo::anim
{

// What the animation code needs to know about one worn item.
struct EquippedItem
{
    bool present      = false;
    uint16_t graphic  = 0; // item graphic (world tile id)
    uint16_t animId   = 0; // tiledata AnimID: the body-animation id used to draw it when worn
    uint16_t hue      = 0;
    bool partialHue   = false; // tiledata PartialHue
    bool isLight      = false; // tiledata LightSource
};

struct MobileEquipment
{
    std::array<EquippedItem, LAYER_COUNT> layers{};

    const EquippedItem* find(Layer layer) const
    {
        const auto i = static_cast<size_t>(layer);
        return i < layers.size() && layers[i].present ? &layers[i] : nullptr;
    }

    EquippedItem& at(Layer layer) { return layers[static_cast<size_t>(layer)]; }

    bool empty() const
    {
        for (const auto& l : layers)
        {
            if (l.present)
            {
                return false;
            }
        }
        return true;
    }
};

namespace PaperdollOrder
{
inline constexpr int N = 0x19; // 25 layers scanned by the reorder helpers

// Back-to-front layer order (later = painted on top) for per-layer AnimIDs `graphic`
// (index = layer, 0 = empty). altTorsoTable selects the female/gargoyle torso table.
void build(std::span<const uint16_t> graphic, bool altTorsoTable, std::span<Layer> order);

// Drops the sentinel and Mount (and Backpack unless asked). Returns the count written.
int filter(std::span<const Layer> order, bool includeBackpack, std::span<Layer> dest);

// Moves Cloak by facing: on top when facing north (0), behind when facing the viewer (3),
// otherwise just below the helmet. Returns the count.
int applyDirectionCloak(std::span<Layer> layers, int count, uint8_t dir);

// In-world draw order for a mobile's equipment. dest must hold N entries.
int buildInWorld(const MobileEquipment& equipment, bool altTorsoTable, uint8_t direction, std::span<Layer> dest);
} // namespace PaperdollOrder

// MobileView.IsCovered: layers hidden under another worn item.
bool isCovered(const MobileEquipment& equipment, uint16_t body, Layer layer);

// MobileView.FixGargoyleEquipments.
uint16_t fixGargoyleEquipment(uint16_t animId);

struct MountInfo
{
    uint16_t graphic = 0; // body drawn for the mount
    int8_t offsetY   = 0; // rider offset
};

// Mounts.TryGet, keyed by the mount item's graphic.
std::optional<MountInfo> findMount(uint16_t itemGraphic);

// Item.GetGraphicForAnimation for a Mount-layer item: the body to draw, from the item graphic
// and its tiledata AnimID.
uint16_t mountGraphicForAnimation(uint16_t itemGraphic, uint16_t itemAnimId);

// The boat-driving "mount" (0x3E96) is not drawn as a mount.
inline constexpr uint16_t BOAT_MOUNT_GRAPHIC = 0x3E96;

// ChairTable: seat data for a static/item graphic. The built-in table ClassicUO ships, or
// the one loaded with loadChairTable.
std::optional<SittingInfoData> findChair(uint16_t graphic);

// Replaces the chair table with the lines of a chair.txt ("graphic,d1,d2,d3,d4,offsetY,mirrorOffsetY",
// '#' or ';' comments). Returns the number of chairs read.
int loadChairTable(std::string_view text);
void resetChairTable();

} // namespace uo::anim
