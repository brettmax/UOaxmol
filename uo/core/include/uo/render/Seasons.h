// SPDX-License-Identifier: BSD-2-Clause
// Ported from ClassicUO (Game/Managers/SeasonManager.cs, Scenes/GameSceneDrawingSorting.cs).
//
// Seasonal graphic replacement: the server's season (0xBC) swaps some land and
// static graphics (snow ground, bare or autumn trees) and hides foliage in
// winter and desolation.

#pragma once

#include <cstdint>
#include <string_view>
#include <unordered_map>

#include "uo/render/WorldSource.h"

namespace uo::render
{

// Same order and values as uo::world::Season and ClassicUO's Season.
enum class SeasonId : uint8_t
{
    Spring,
    Summer,
    Fall,
    Winter,
    Desolation,
};

class SeasonTable
{
public:
    // ClassicUO's default seasons.txt.
    static const SeasonTable& defaults();

    SeasonTable() = default;

    // Parses seasons.txt lines: "<season>,<static|landtile>,<original>,<replacement>",
    // numbers decimal or 0x hex; '#' and '//' lines are comments. Later lines win.
    void parse(std::string_view text);
    void set(SeasonId season, bool land, uint16_t original, uint16_t replacement);

    uint16_t staticGraphic(SeasonId season, uint16_t graphic) const;
    uint16_t landGraphic(SeasonId season, uint16_t graphic) const;

private:
    static uint32_t key(SeasonId season, bool land, uint16_t graphic)
    {
        return (static_cast<uint32_t>(season) << 17) | (land ? 1u << 16 : 0u) | graphic;
    }

    std::unordered_map<uint32_t, uint16_t> _map;
};

// GameSceneDrawingSorting.IsFoliageVisibleAtSeason.
inline bool foliageVisibleAtSeason(const StaticTileData& data, SeasonId season)
{
    return !(data.is(assets::TF_Foliage) && !data.is(assets::TF_MultiMovable) && season >= SeasonId::Winter);
}

}  // namespace uo::render
