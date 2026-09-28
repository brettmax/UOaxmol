// SPDX-License-Identifier: BSD-2-Clause
#include "uo/assets/Map.h"

#include "uo/io/BinaryReader.h"

#include <filesystem>

namespace uo::assets
{

std::array<int, 2> MapFacet::defaultSize(int mapIndex)
{
    static constexpr std::array<std::array<int, 2>, 6> sizes = {{
        {7168, 4096},  // Felucca
        {7168, 4096},  // Trammel
        {2304, 1600},  // Ilshenar
        {2560, 2048},  // Malas
        {1448, 1448},  // Tokuno
        {1280, 4096},  // Ter Mur
    }};
    if (mapIndex < 0 || mapIndex >= static_cast<int>(sizes.size()))
        return {0, 0};
    return sizes[mapIndex];
}

bool MapFacet::load(const std::string& dir, int mapIndex)
{
    namespace fs = std::filesystem;
    auto p       = [&](const std::string& name) { return (fs::path(dir) / name).string(); };
    std::string i = std::to_string(mapIndex);

    std::string uop = p("map" + i + "LegacyMUL.uop");
    std::string mul = p("map" + i + ".mul");
    if (!fs::exists(uop))
        uop.clear();

    auto size = defaultSize(mapIndex);
    if (mapIndex <= 1)
    {
        std::error_code ec;
        std::uintmax_t bytes = fs::exists(uop) ? 0 : fs::file_size(mul, ec);
        // 393216 blocks == 6144x4096: the pre-Mondain's Legacy (and so T2A) Britannia.
        if (!ec && bytes / kLandBlockBytes == 393216)
            size[0] = 6144;
    }

    return loadFiles(mul, uop, p("staidx" + i + ".mul"), p("statics" + i + ".mul"), size[0], size[1]);
}

bool MapFacet::loadFiles(const std::string& mapMul, const std::string& mapUop, const std::string& staidx,
                         const std::string& statics, int widthTiles, int heightTiles)
{
    _width  = widthTiles;
    _height = heightTiles;
    _mapUop.reset();

    if (!mapUop.empty())
    {
        int index = 0;
        auto slash = mapUop.find_last_of("/\\");
        std::string file = mapUop.substr(slash == std::string::npos ? 0 : slash + 1);
        if (file.size() > 3 && file.compare(0, 3, "map") == 0)
            index = std::atoi(file.c_str() + 3);
        _mapUop = std::make_unique<io::UopFile>(mapUop, "build/map" + std::to_string(index) + "legacymul/%08u.dat");
        if (!_mapUop->load())
            return false;
    }
    else if (!_mapMul.open(mapMul))
    {
        return false;
    }

    // Statics are optional: a facet without them is just bare land.
    _staidx.open(staidx);
    _statics.open(statics);
    return true;
}

std::span<const std::uint8_t> MapFacet::landBlockBytes(int bx, int by) const
{
    if (bx < 0 || by < 0 || bx >= blockWidth() || by >= blockHeight())
        return {};
    std::size_t block = static_cast<std::size_t>(bx) * blockHeight() + by;

    if (_mapUop)
    {
        const io::FileIndex* e = _mapUop->entry(block / kUopBlocksPerEntry);
        if (!e)
            return {};
        std::size_t off = static_cast<std::size_t>(e->offset) + (block % kUopBlocksPerEntry) * kLandBlockBytes;
        return _mapUop->data().slice(off, kLandBlockBytes);
    }
    return _mapMul.slice(block * kLandBlockBytes, kLandBlockBytes);
}

std::array<LandCell, 64> MapFacet::landBlock(int bx, int by) const
{
    std::array<LandCell, 64> cells{};
    auto raw = landBlockBytes(bx, by);
    if (raw.size() < kLandBlockBytes)
        return cells;
    io::BinaryReader r(raw);
    r.readU32LE();  // header
    for (auto& c : cells)
    {
        c.tileId = r.readU16LE();
        c.z      = r.readI8();
    }
    return cells;
}

LandCell MapFacet::land(int x, int y) const
{
    if (x < 0 || y < 0 || x >= _width || y >= _height)
        return {};
    return landBlock(x >> 3, y >> 3)[(y & 7) * 8 + (x & 7)];
}

std::vector<StaticCell> MapFacet::staticsBlock(int bx, int by) const
{
    std::vector<StaticCell> out;
    if (bx < 0 || by < 0 || bx >= blockWidth() || by >= blockHeight())
        return out;

    std::size_t block = static_cast<std::size_t>(bx) * blockHeight() + by;
    io::BinaryReader idx(_staidx.slice(block * 12, 12));
    std::uint32_t offset = idx.readU32LE();
    std::int32_t length  = idx.readI32LE();
    if (idx.overflowed() || offset == 0xFFFFFFFFu || length <= 0)
        return out;

    io::BinaryReader r(_statics.slice(offset, static_cast<std::size_t>(length)));
    std::size_t count = r.size() / kStaticBytes;
    out.reserve(count);
    for (std::size_t i = 0; i < count; ++i)
    {
        StaticCell s;
        s.graphic = r.readU16LE();
        s.x       = r.readU8();
        s.y       = r.readU8();
        s.z       = r.readI8();
        s.hue     = r.readU16LE();
        if (s.x < 8 && s.y < 8)
            out.push_back(s);
    }
    return out;
}

}  // namespace uo::assets
