// SPDX-License-Identifier: BSD-2-Clause
// Maps: uocore's MapFacet reads the land blocks and statics; this re-tiles them into 64x64
// chunks in a single .uomap file per facet, applying verdata patches to map 0 the way
// ClassicUO's UOFileManager does.
#include "Png.h"
#include "Stages.h"
#include "UoMap.h"

#include "uo/assets/Color.h"
#include "uo/assets/Map.h"
#include "uo/io/BinaryReader.h"

#include <algorithm>
#include <filesystem>

namespace fs = std::filesystem;
using uo::assets::LandCell;
using uo::assets::MapFacet;
using uo::assets::StaticCell;
using uo::io::Verdata;

namespace uoconvert
{

namespace
{

// A verdata map patch holds the same bytes as the block it replaces (196 for land, 7 per
// static), so it is read with the same layout MapFacet uses.
std::array<LandCell, 64> landFromBytes(std::span<const std::uint8_t> raw)
{
    std::array<LandCell, 64> cells{};
    if (raw.size() < MapFacet::kLandBlockBytes)
        return cells;
    uo::io::BinaryReader r(raw);
    r.readU32LE();
    for (auto& c : cells)
    {
        c.tileId = r.readU16LE();
        c.z      = r.readI8();
    }
    return cells;
}

std::vector<StaticCell> staticsFromBytes(std::span<const std::uint8_t> raw)
{
    std::vector<StaticCell> out;
    uo::io::BinaryReader r(raw);
    std::size_t n = std::min<std::size_t>(raw.size() / MapFacet::kStaticBytes, 1024);
    for (std::size_t i = 0; i < n; ++i)
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

}  // namespace

bool runMaps(Context& ctx, JsonWriter& m)
{
    std::vector<std::uint16_t> radarcol;
    if (std::string rc = ctx.data.find("radarcol.mul"); !rc.empty() && ctx.opt.radar)
    {
        auto bytes = Context::readFile(rc);
        radarcol.resize(bytes.size() / 2);
        for (std::size_t i = 0; i < radarcol.size(); ++i)
            radarcol[i] = static_cast<std::uint16_t>(bytes[i * 2] | (bytes[i * 2 + 1] << 8));
    }
    auto radarColor = [&](std::size_t i) -> std::uint32_t {
        return i < radarcol.size() ? uo::assets::color16To32(radarcol[i]) | uo::assets::kOpaque : uo::assets::kOpaque;
    };

    int converted = 0;
    m.key("facets").beginArray();
    for (int index = 0; index < 6; ++index)
    {
        std::string i   = std::to_string(index);
        std::string mul = ctx.data.find("map" + i + ".mul");
        std::string uop = ctx.opt.useUop ? ctx.data.find("map" + i + "LegacyMUL.uop") : "";
        if (mul.empty() && uop.empty())
            continue;

        auto size = MapFacet::defaultSize(index);
        std::error_code ec;
        // 393,216 blocks is the 6144-wide Britannia of every pre-Mondain's Legacy client, T2A included.
        if (index <= 1 && uop.empty() && fs::file_size(mul, ec) / MapFacet::kLandBlockBytes == 393216)
            size[0] = 6144;

        MapFacet facet;
        if (!facet.loadFiles(mul, uop, ctx.data.find("staidx" + i + ".mul"), ctx.data.find("statics" + i + ".mul"),
                             size[0], size[1]))
        {
            ctx.warn("map" + i + " could not be opened");
            continue;
        }

        const int bw = facet.blockWidth(), bh = facet.blockHeight();
        const int perChunk = UoMapChunk::kSize / MapFacet::kBlockSize;  // 8 blocks
        UoMapHeader h;
        h.mapIndex = static_cast<std::uint16_t>(index);
        h.width    = static_cast<std::uint32_t>(facet.width());
        h.height   = static_cast<std::uint32_t>(facet.height());
        h.chunksX  = static_cast<std::uint32_t>((bw + perChunk - 1) / perChunk);
        h.chunksY  = static_cast<std::uint32_t>((bh + perChunk - 1) / perChunk);

        UoMapWriter writer;
        std::string path = ctx.out("maps/map" + i + ".uomap");
        writer.open(path, h);

        const bool patchable = index == 0;
        std::size_t landPatches = 0, staticPatches = 0, staticCount = 0;
        std::vector<std::uint32_t> radar;
        if (!radarcol.empty())
            radar.assign(static_cast<std::size_t>(facet.width()) * facet.height(), uo::assets::kOpaque);

        for (std::uint32_t cy = 0; cy < h.chunksY; ++cy)
        {
            for (std::uint32_t cx = 0; cx < h.chunksX; ++cx)
            {
                UoMapChunk chunk;
                for (int oy = 0; oy < perChunk; ++oy)
                {
                    for (int ox = 0; ox < perChunk; ++ox)
                    {
                        int bx = static_cast<int>(cx) * perChunk + ox, by = static_cast<int>(cy) * perChunk + oy;
                        if (bx >= bw || by >= bh)
                            continue;
                        auto block = static_cast<std::uint32_t>(bx * bh + by);

                        std::array<LandCell, 64> land;
                        const Verdata::Patch* lp = patchable ? ctx.verdata.find(Verdata::Map0, block) : nullptr;
                        if (lp && lp->length >= MapFacet::kLandBlockBytes)
                        {
                            land = landFromBytes(ctx.verdata.bytes(*lp));
                            ++landPatches;
                        }
                        else
                            land = facet.landBlock(bx, by);

                        std::vector<StaticCell> statics;
                        if (const Verdata::Patch* sp = patchable ? ctx.verdata.find(Verdata::Statics0, block) : nullptr)
                        {
                            statics = staticsFromBytes(ctx.verdata.bytes(*sp));
                            ++staticPatches;
                        }
                        else
                            statics = facet.staticsBlock(bx, by);

                        for (int c = 0; c < 64; ++c)
                        {
                            int x = ox * 8 + (c & 7), y = oy * 8 + (c >> 3);
                            chunk.land[y * UoMapChunk::kSize + x] = land[c].tileId;
                            chunk.z[y * UoMapChunk::kSize + x]    = land[c].z;
                        }
                        for (const StaticCell& s : statics)
                            chunk.statics.push_back({static_cast<std::uint8_t>(ox * 8 + s.x),
                                                     static_cast<std::uint8_t>(oy * 8 + s.y), s.z, s.graphic, s.hue});
                    }
                }
                std::stable_sort(chunk.statics.begin(), chunk.statics.end(),
                                 [](const UoMapStatic& a, const UoMapStatic& b) {
                                     return a.y != b.y ? a.y < b.y : a.x < b.x;
                                 });
                staticCount += chunk.statics.size();

                if (!radar.empty())
                {
                    // Land colour, overdrawn by the highest static in the cell (the later one on ties).
                    std::vector<int> topZ(UoMapChunk::kSize * UoMapChunk::kSize, -1000);
                    std::vector<std::uint32_t> col(topZ.size());
                    for (std::size_t c = 0; c < col.size(); ++c)
                        col[c] = radarColor(chunk.land[c]);
                    for (const auto& s : chunk.statics)
                    {
                        std::size_t c = static_cast<std::size_t>(s.y) * UoMapChunk::kSize + s.x;
                        if (s.z >= topZ[c])
                        {
                            topZ[c] = s.z;
                            col[c]  = radarColor(0x4000u + s.graphic);
                        }
                    }
                    for (int y = 0; y < UoMapChunk::kSize; ++y)
                    {
                        int gy = static_cast<int>(cy) * UoMapChunk::kSize + y;
                        if (gy >= facet.height())
                            break;
                        for (int x = 0; x < UoMapChunk::kSize; ++x)
                        {
                            int gx = static_cast<int>(cx) * UoMapChunk::kSize + x;
                            if (gx < facet.width())
                                radar[static_cast<std::size_t>(gy) * facet.width() + gx] = col[y * UoMapChunk::kSize + x];
                        }
                    }
                }
                writer.write(cx, cy, chunk);
            }
        }
        writer.finish();
        if (!radar.empty())
            writePng(ctx.out("maps/map" + i + "-radar.png"), facet.width(), facet.height(), radar.data());

        JsonWriter meta(true);
        meta.beginObject().field("version", 1).field("map", index).field("file", "map" + i + ".uomap");
        meta.field("width", facet.width()).field("height", facet.height()).field("chunkSize", UoMapChunk::kSize);
        meta.field("chunksX", h.chunksX).field("chunksY", h.chunksY).field("statics", static_cast<std::uint64_t>(staticCount));
        if (!radar.empty())
            meta.field("radar", "map" + i + "-radar.png");
        meta.endObject().save(ctx.out("maps/map" + i + ".json"));

        m.beginObject().field("map", index).field("width", facet.width()).field("height", facet.height());
        m.field("statics", static_cast<std::uint64_t>(staticCount)).field("source", uop.empty() ? "mul" : "uop");
        m.field("verdataLandBlocks", static_cast<std::uint64_t>(landPatches));
        m.field("verdataStaticBlocks", static_cast<std::uint64_t>(staticPatches));
        m.endObject();
        ++converted;
        std::printf("          map%d %dx%d, %zu statics\n", index, facet.width(), facet.height(), staticCount);
    }
    m.endArray();
    if (!converted)
    {
        m.field("skipped", "no map*.mul or map*LegacyMUL.uop");
        return false;
    }
    return true;
}

}  // namespace uoconvert
