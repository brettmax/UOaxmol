// SPDX-License-Identifier: BSD-2-Clause
// Part of the AxmolUO client core. Ported from ClassicUO.Assets (MapLoader).
#pragma once

#include "uo/io/MappedFile.h"
#include "uo/io/UOFile.h"

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace uo::assets
{

struct LandCell
{
    std::uint16_t tileId = 0;
    std::int8_t z        = 0;
};

struct StaticCell
{
    std::uint16_t graphic = 0;
    std::uint8_t x        = 0;  // 0..7 within the block
    std::uint8_t y        = 0;
    std::int8_t z         = 0;
    std::uint16_t hue     = 0;
};

// One map facet (map0 = Felucca, map1 = Trammel, ...). The world is split into 8x8 blocks,
// stored column-major: block (bx, by) is record bx * blockHeight + by.
class MapFacet
{
public:
    static constexpr int kBlockSize      = 8;
    static constexpr int kLandBlockBytes = 4 + 64 * 3;  // 196
    static constexpr int kStaticBytes    = 7;
    static constexpr int kUopBlocksPerEntry = 4096;

    // Default facet sizes in tiles, indexed by map number (ClassicUO's MapsDefaultSize).
    static std::array<int, 2> defaultSize(int mapIndex);

    // Opens map{i}.mul (or map{i}LegacyMUL.uop), staidx{i}.mul and statics{i}.mul from `dir`.
    // Map 0/1 width is derived from the file (6144 for T2A-era installs, 7168 after ML).
    bool load(const std::string& dir, int mapIndex);

    // Explicit paths, used by tests. uopPath may be empty.
    bool loadFiles(const std::string& mapMul, const std::string& mapUop, const std::string& staidx,
                   const std::string& statics, int widthTiles, int heightTiles);

    int width() const { return _width; }
    int height() const { return _height; }
    int blockWidth() const { return _width / kBlockSize; }
    int blockHeight() const { return _height / kBlockSize; }

    // The 64 land cells of a block, row-major (index = y * 8 + x). Empty when out of range.
    std::array<LandCell, 64> landBlock(int bx, int by) const;
    LandCell land(int x, int y) const;
    std::vector<StaticCell> staticsBlock(int bx, int by) const;

private:
    std::span<const std::uint8_t> landBlockBytes(int bx, int by) const;

    int _width  = 0;
    int _height = 0;
    io::MappedFile _mapMul;
    std::unique_ptr<io::UopFile> _mapUop;
    io::MappedFile _staidx;
    io::MappedFile _statics;
};

}  // namespace uo::assets
