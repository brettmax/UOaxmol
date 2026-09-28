// SPDX-License-Identifier: BSD-2-Clause
// Ported from ClassicUO (ClassicUO.Client/Game/GameObjects/Land.cs).
//
// A land tile whose corners sit at different heights is drawn as a quad built
// from its texmap texture instead of the flat 44x44 art diamond. This computes
// the four corner offsets, the per-corner lighting normals, and the Z values the
// sorter uses.

#pragma once

#include <cstdint>

#include "uo/render/WorldSource.h"

namespace uo::render
{

struct Vec3
{
    float x = 0, y = 0, z = 0;
};

// Corner heights in screen pixels (Z * 4). Same layout as ClassicUO's
// UltimaBatcher2D.YOffsets.
struct YOffsets
{
    int top = 0, right = 0, left = 0, bottom = 0;
};

struct LandStretch
{
    bool stretched = false;
    int8_t averageZ = 0;
    int8_t minZ     = 0;
    YOffsets offsets;
    Vec3 normalTop{0, 0, 1}, normalRight{0, 0, 1}, normalLeft{0, 0, 1}, normalBottom{0, 0, 1};
};

// canStretch is false for tiles ClassicUO never stretches: no texmap entry, or
// texId 0 on a wet tile (water keeps its animated art).
LandStretch computeLandStretch(const IMapSource& map, int x, int y, int8_t z, bool canStretch);

bool canStretchLand(const LandTileData& data, const ITileData& tiles);

}  // namespace uo::render
