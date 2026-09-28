// SPDX-License-Identifier: BSD-2-Clause
// Part of the AxmolUO client core.
//
// Data the world renderer reads. Narrow interfaces so the sorter and geometry
// builder can be tested on synthetic maps; AssetsWorldSource implements them
// over a loaded installation.

#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

#include "uo/assets/TileData.h"

namespace uo::assets
{
class MapFacet;
class Texmaps;
}  // namespace uo::assets

namespace uo::render
{

// Returned by IMapSource::landZ for coordinates outside the map, matching
// ClassicUO's Map.GetTileZ.
inline constexpr std::int8_t kInvalidLandZ = -125;

struct LandCell
{
    std::uint16_t graphic = 0;  // already masked with 0x3FFF
    std::int8_t z         = 0;
};

struct StaticEntry
{
    std::uint16_t graphic = 0;
    std::uint8_t x        = 0;  // 0..7 within the block
    std::uint8_t y        = 0;  // 0..7 within the block
    std::int8_t z         = 0;
    std::uint16_t hue     = 0;
};

class IMapSource
{
public:
    virtual ~IMapSource() = default;

    // Map size in tiles.
    virtual int width() const  = 0;
    virtual int height() const = 0;

    // Land cell at a tile. Returns false when (x, y) is off the map.
    virtual bool land(int x, int y, LandCell& out) const = 0;

    // Statics of an 8x8 block, in file order. `out` is appended to.
    virtual void statics(int blockX, int blockY, std::vector<StaticEntry>& out) const = 0;

    std::int8_t landZ(int x, int y) const
    {
        LandCell c;
        return (x >= 0 && y >= 0 && land(x, y, c)) ? c.z : kInvalidLandZ;
    }
};

struct LandTileData
{
    std::uint64_t flags = 0;
    std::uint16_t texId = 0;
};

struct StaticTileData
{
    std::uint64_t flags        = 0;
    std::uint8_t height        = 0;
    std::string_view name      = {};

    bool is(assets::TileFlag f) const { return (flags & f) != 0; }
};

class ITileData
{
public:
    virtual ~ITileData() = default;

    virtual LandTileData land(std::uint16_t graphic) const   = 0;
    virtual StaticTileData item(std::uint16_t graphic) const = 0;
    virtual int itemCount() const                            = 0;

    // True when texmaps has a valid entry for texId (ClassicUO checks the
    // index entry length). A land tile without one cannot be stretched.
    virtual bool hasTexmap(std::uint16_t texId) const = 0;
};

// Both interfaces over loaded assets. Everything passed in must outlive it;
// texmaps may be null (then nothing is stretched).
class AssetsWorldSource final : public IMapSource, public ITileData
{
public:
    AssetsWorldSource(const assets::MapFacet& map, const assets::TileData& tiles, const assets::Texmaps* texmaps)
        : _map(map), _tiles(tiles), _texmaps(texmaps)
    {}

    int width() const override;
    int height() const override;
    bool land(int x, int y, LandCell& out) const override;
    void statics(int blockX, int blockY, std::vector<StaticEntry>& out) const override;

    LandTileData land(std::uint16_t graphic) const override;
    StaticTileData item(std::uint16_t graphic) const override;
    int itemCount() const override;
    bool hasTexmap(std::uint16_t texId) const override;

    using IMapSource::landZ;

private:
    const assets::MapFacet& _map;
    const assets::TileData& _tiles;
    const assets::Texmaps* _texmaps;
};

}  // namespace uo::render
