// SPDX-License-Identifier: BSD-2-Clause
// Image stages: art, gumps, texmaps, lights and hues. Every pixel is decoded by uocore; this
// file only chooses what to export, applies verdata and the shard's override folders, and
// hands the images to the atlas writer.
#include "Atlas.h"
#include "Png.h"
#include "Stages.h"

#include "uo/assets/Art.h"
#include "uo/assets/Color.h"
#include "uo/assets/Gumps.h"
#include "uo/assets/Hues.h"
#include "uo/assets/Lights.h"
#include "uo/assets/Texmaps.h"

#include <cstdio>
#include <filesystem>
#include <map>
#include <tuple>

namespace fs = std::filesystem;
using uo::assets::Image;
using uo::io::Verdata;

namespace uoconvert
{

namespace
{

std::string hexName(const char* prefix, std::uint32_t id)
{
    char buf[48];
    std::snprintf(buf, sizeof(buf), "%s/0x%04X", prefix, id);
    return buf;
}

// Loose files a shard ships next to the client, keyed by the number in their file name:
// ModernUO-Client reads Art/Land/<id>.art, Art/Statics/<id>.art and Gumps/<id>.gump before
// the archives, so the converted output has to carry them too.
std::map<std::uint32_t, std::vector<std::uint8_t>> gatherOverrides(const Context& ctx, const std::string& folder,
                                                                   const std::string& ext, std::uint32_t limit)
{
    std::map<std::uint32_t, std::vector<std::uint8_t>> found;
    for (const std::string& rel : ctx.overrides.list(folder, ext))
    {
        std::string stem = fs::path(rel).stem().string();
        if (stem.empty() || stem.find_first_not_of("0123456789") != std::string::npos || stem.size() > 9)
            continue;
        auto id = static_cast<std::uint32_t>(std::stoul(stem));
        // Only direct children: Art/Land/x.art, not Art/Land/old/x.art.
        if (id < limit && fs::path(rel).parent_path().generic_string().size() == folder.size())
            found[id] = Context::readFile((fs::path(ctx.overrides.root()) / rel).string());
    }
    return found;
}

AtlasOptions atlasOptions(const Context& ctx)
{
    AtlasOptions o;
    o.maxSize = ctx.opt.maxSize;
    o.jobs    = ctx.opt.jobs;
    return o;
}

void writeResult(JsonWriter& m, const AtlasResult& r, const char* what)
{
    m.field(what, static_cast<std::uint64_t>(r.frames));
    m.field(std::string(what) + "Sheets", static_cast<std::uint64_t>(r.sheets));
    if (!r.missing.empty())
        m.field(std::string(what) + "Undecodable", static_cast<std::uint64_t>(r.missing.size()));
}

std::string aliasesJson(const std::map<long, std::pair<long, long>>& aliases, bool withHue)
{
    JsonWriter j;
    j.beginObject();
    for (const auto& [id, src] : aliases)
    {
        j.key(std::to_string(id)).beginObject().field("source", static_cast<std::int64_t>(src.first));
        if (withHue)
            j.field("hue", static_cast<std::int64_t>(src.second));
        j.endObject();
    }
    j.endObject();
    return "\"aliases\":" + j.str();
}

}  // namespace

bool runArt(Context& ctx, JsonWriter& m)
{
    using uo::assets::Art;
    auto file = ctx.openIndexed("artLegacyMUL.uop", "build/artlegacymul/%08u.tga", "art.mul", "artidx.mul");
    auto ourLand    = gatherOverrides(ctx, "Art/Land", ".art", Art::kMaxLand);
    auto ourStatics = gatherOverrides(ctx, "Art/Statics", ".art", Art::kMaxStatic);
    if (!file && ourLand.empty() && ourStatics.empty())
    {
        m.field("skipped", "no art.mul/artidx.mul or artLegacyMUL.uop");
        return false;
    }
    std::vector<Sprite> land, statics;
    std::size_t compressed = 0;
    for (std::uint32_t id = 0; id < Art::kMaxLand; ++id)
    {
        bool ours = ourLand.count(id) && ourLand[id].size() >= 2024;
        if (ours || ctx.entry(file.get(), id, Verdata::Art).bytes.size() >= 2024)
            land.push_back({id, 44, 44, hexName("land", id)});
    }
    for (std::uint32_t g = 0; g < Art::kMaxStatic; ++g)
    {
        std::span<const std::uint8_t> head;
        if (auto it = ourStatics.find(g); it != ourStatics.end())
            head = it->second;
        EntryBytes e;
        if (head.size() < 8)
        {
            e    = ctx.entry(file.get(), g + Art::kMaxLand, Verdata::Art);
            head = e.bytes;
            if (e.compressed)
            {
                ++compressed;
                continue;
            }
        }
        if (head.size() < 8)
            continue;
        int w = static_cast<std::int16_t>(head[4] | (head[5] << 8));
        int h = static_cast<std::int16_t>(head[6] | (head[7] << 8));
        if (w <= 0 || h <= 0 || w > 1024 || h > 1024)
        {
            ctx.warn("static " + hexName("art", g) + " claims " + std::to_string(w) + "x" + std::to_string(h));
            continue;
        }
        statics.push_back({g, w, h, hexName("static", g)});
    }

    auto decodeLand = [&](const Sprite& s) -> Image {
        if (auto it = ourLand.find(s.id); it != ourLand.end())
            if (Image img = Art::decodeLand(it->second); !img.empty())
                return img;
        return Art::decodeLand(ctx.entry(file.get(), s.id, Verdata::Art).bytes);
    };
    auto decodeStatic = [&](const Sprite& s) -> Image {
        if (auto it = ourStatics.find(s.id); it != ourStatics.end())
            if (Image img = Art::decodeStatic(it->second); !img.empty())
                return img;
        return Art::decodeStatic(ctx.entry(file.get(), s.id + Art::kMaxLand, Verdata::Art).bytes);
    };

    std::string dir = ctx.outDir("art");
    writeResult(m, writeAtlas(dir, "land", land, decodeLand, atlasOptions(ctx)), "land");
    writeResult(m, writeAtlas(dir, "statics", statics, decodeStatic, atlasOptions(ctx)), "statics");
    m.field("landOverrides", static_cast<std::uint64_t>(ourLand.size()));
    m.field("staticOverrides", static_cast<std::uint64_t>(ourStatics.size()));
    if (compressed)
        m.field("compressedSkipped", static_cast<std::uint64_t>(compressed));
    return true;
}

bool runGumps(Context& ctx, JsonWriter& m)
{
    using uo::assets::Gumps;
    constexpr std::uint32_t kMaxGump = 0x10000;
    auto file = ctx.openIndexed("gumpartLegacyMUL.uop", "build/gumpartlegacymul/%08u.tga", "gumpart.mul",
                                "gumpidx.mul", true);
    auto ours = gatherOverrides(ctx, "Gumps", ".gump", kMaxGump);
    if (!file && ours.empty())
    {
        m.field("skipped", "no gumpart.mul/gumpidx.mul or gumpartLegacyMUL.uop");
        return false;
    }
    auto sizeOf = [](std::span<const std::uint8_t> b) {
        return std::pair<std::uint32_t, std::uint32_t>(
            b.size() >= 8 ? b[0] | (b[1] << 8) | (b[2] << 16) | (static_cast<std::uint32_t>(b[3]) << 24) : 0,
            b.size() >= 8 ? b[4] | (b[5] << 8) | (b[6] << 16) | (static_cast<std::uint32_t>(b[7]) << 24) : 0);
    };

    std::vector<Sprite> sprites;
    std::set<long> present;
    std::size_t compressed = 0;
    for (std::uint32_t id = 0; id < kMaxGump; ++id)
    {
        int w = 0, h = 0;
        if (auto it = ours.find(id); it != ours.end())
        {
            auto [ow, oh] = sizeOf(it->second);
            if (ow > 0 && oh > 0 && ow <= 4096 && oh <= 4096)
                w = static_cast<int>(ow), h = static_cast<int>(oh);
            else
                ctx.warn("Gumps/" + std::to_string(id) + ".gump claims " + std::to_string(ow) + "x" +
                         std::to_string(oh) + "; using the archive");
        }
        if (!w)
        {
            EntryBytes e = ctx.entry(file.get(), id, Verdata::Gumps);
            if (e.compressed)
            {
                ++compressed;
                continue;
            }
            if (e.inflated)
                std::tie(w, h) = sizeOf(e.bytes);
            else if (!e.bytes.empty())
                w = e.width, h = e.height;
        }
        if (w > 0 && h > 0 && w <= 4096 && h <= 4096)
        {
            sprites.push_back({id, w, h, hexName("gump", id)});
            present.insert(id);
        }
    }

    // gump.def only fills numbers the archive lacks: "<new> {<candidates>} <hue>", first
    // existing candidate wins. The hue is left to the client's hue shader, not baked in.
    std::map<long, std::pair<long, long>> aliases;
    for (const DefLine& d : readDef(ctx.data.find("gump.def")))
    {
        if (d.before.size() != 1 || !d.hasGroup || present.count(d.before[0]) || d.before[0] < 0 ||
            d.before[0] >= static_cast<long>(kMaxGump))
            continue;
        for (long c : d.group)
        {
            if (present.count(c))
            {
                aliases[d.before[0]] = {c, d.after.empty() ? 0 : d.after[0]};
                break;
            }
        }
    }

    auto decode = [&](const Sprite& s) -> Image {
        if (auto it = ours.find(s.id); it != ours.end())
        {
            std::span<const std::uint8_t> b = it->second;
            if (b.size() > 8)
                if (Image img = Gumps::decode(b.subspan(8), s.w, s.h); !img.empty())
                    return img;
        }
        EntryBytes e = ctx.entry(file.get(), s.id, Verdata::Gumps);
        if (e.inflated)  // compressed UOP gumps carry { u32 width; u32 height } ahead of the rows
            return e.bytes.size() > 8 ? Gumps::decode(e.bytes.subspan(8), s.w, s.h) : Image{};
        return Gumps::decode(e.bytes, e.width, e.height);
    };

    AtlasOptions o = atlasOptions(ctx);
    o.indexExtraJson = aliasesJson(aliases, true);
    writeResult(m, writeAtlas(ctx.outDir("gumps"), "gumps", sprites, decode, o), "gumps");
    m.field("aliases", static_cast<std::uint64_t>(aliases.size()));
    m.field("overrides", static_cast<std::uint64_t>(ours.size()));
    if (compressed)
        m.field("compressedSkipped", static_cast<std::uint64_t>(compressed));
    return true;
}

bool runTexmaps(Context& ctx, JsonWriter& m)
{
    using uo::assets::Texmaps;
    auto file = ctx.openIndexed("", "", "texmaps.mul", "texidx.mul");
    if (!file)
    {
        m.field("skipped", "no texmaps.mul/texidx.mul");
        return false;
    }
    std::vector<Sprite> sprites;
    std::set<long> present;
    for (std::uint32_t id = 0; id < file->count(); ++id)
    {
        EntryBytes e = ctx.entry(file.get(), id, -1);  // ClassicUO applies no verdata to texmaps
        if (e.bytes.empty())
            continue;
        int size = Texmaps::sizeFor(e.bytes.size());
        sprites.push_back({id, size, size, hexName("texmap", id)});
        present.insert(id);
    }

    // TexTerr.def: "<index> {<candidates>}"; ClassicUO keeps the last candidate that exists.
    std::map<long, std::pair<long, long>> aliases;
    for (const DefLine& d : readDef(ctx.data.find("TexTerr.def")))
    {
        if (d.before.size() != 1 || !d.hasGroup)
            continue;
        for (long c : d.group)
            if (present.count(c))
                aliases[d.before[0]] = {c, 0};
    }

    auto decode = [&](const Sprite& s) { return Texmaps::decode(ctx.entry(file.get(), s.id, -1).bytes); };
    AtlasOptions o = atlasOptions(ctx);
    o.padding = 0;
    o.extrude = 1;  // stretched land samples these with linear filtering; repeat the edges
    o.indexExtraJson = aliasesJson(aliases, false);
    writeResult(m, writeAtlas(ctx.outDir("texmaps"), "texmaps", sprites, decode, o), "texmaps");
    m.field("aliases", static_cast<std::uint64_t>(aliases.size()));
    return true;
}

bool runLights(Context& ctx, JsonWriter& m)
{
    using uo::assets::Lights;
    auto file = ctx.openIndexed("", "", "light.mul", "lightidx.mul");
    if (!file)
    {
        m.field("skipped", "no light.mul/lightidx.mul");
        return false;
    }
    std::vector<Sprite> sprites;
    for (std::uint32_t id = 0; id < file->count(); ++id)
    {
        EntryBytes e = ctx.entry(file.get(), id, -1);
        if (!e.bytes.empty() && e.width > 0 && e.height > 0)
            sprites.push_back({id, e.width, e.height, "light/" + std::to_string(id)});
    }
    auto decode = [&](const Sprite& s) {
        EntryBytes e = ctx.entry(file.get(), s.id, -1);
        return Lights::decode(e.bytes, e.width, e.height);
    };
    writeResult(m, writeAtlas(ctx.outDir("lights"), "lights", sprites, decode, atlasOptions(ctx)), "lights");
    return true;
}

bool runHues(Context& ctx, JsonWriter& m)
{
    std::string huesPath = ctx.data.find("hues.mul");
    if (huesPath.empty())
    {
        m.field("skipped", "no hues.mul");
        return false;
    }
    std::vector<std::uint8_t> hues  = Context::readFile(huesPath);
    std::vector<std::uint8_t> radar = Context::readFile(ctx.data.find("radarcol.mul"));

    // Verdata FileID 32 replaces a group's header and colour tables (not its names), as
    // UOFileManager does.
    constexpr std::size_t kGroup = 4 + 8 * 88;
    std::size_t patched = 0;
    for (const auto& p : ctx.verdata.patches())
    {
        if (p.fileId != Verdata::Hues)
            continue;
        auto src       = ctx.verdata.bytes(p);
        std::size_t at = static_cast<std::size_t>(p.blockId) * kGroup;
        if (src.size() < kGroup || at + kGroup > hues.size())
            continue;
        std::copy_n(src.begin(), 4, hues.begin() + at);
        for (std::size_t e = 0; e < 8; ++e)
            std::copy_n(src.begin() + 4 + e * 88, 64, hues.begin() + at + 4 + e * 88);
        ++patched;
    }

    uo::assets::Hues h;
    h.loadFromBytes(hues.data(), hues.size(), radar.data(), radar.size());
    if (h.count() == 0)
    {
        m.field("skipped", "hues.mul holds no hues");
        return false;
    }
    std::vector<std::uint32_t> texture = h.buildHueTexture();
    writePng(ctx.out("hues/hues.png"), 32, static_cast<int>(h.count()), texture.data());

    JsonWriter j;
    j.beginObject().field("version", 1).field("texture", "hues.png").field("count", static_cast<std::uint64_t>(h.count()));
    j.field("layout", "row r is hue r+1; sample column (source colour >> 10) & 31, i.e. its 5-bit red channel");
    j.key("hues").beginArray();
    for (std::size_t i = 0; i < h.count(); ++i)
    {
        const auto* e = h.entry(static_cast<std::uint16_t>(i + 1));
        j.beginArray().value(latin1ToUtf8(e->name)).value(e->tableStart).value(e->tableEnd).endArray();
    }
    j.endArray().field("columns", "name, tableStart, tableEnd").endObject();
    j.save(ctx.out("hues/hues.json"));

    m.field("hues", static_cast<std::uint64_t>(h.count())).field("verdataPatches", static_cast<std::uint64_t>(patched));

    if (!radar.empty())
    {
        std::size_t n    = radar.size() / 2;
        const int width  = 256;
        const int height = static_cast<int>((n + width - 1) / width);
        std::vector<std::uint32_t> px(static_cast<std::size_t>(width) * height, 0);
        for (std::size_t i = 0; i < n; ++i)
            px[i] = uo::assets::color16To32(h.radarColor(i)) | uo::assets::kOpaque;
        writePng(ctx.out("hues/radarcol.png"), width, height, px.data());
        m.field("radarColors", static_cast<std::uint64_t>(n));
    }
    return true;
}

}  // namespace uoconvert
