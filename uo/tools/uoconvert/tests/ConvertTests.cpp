// SPDX-License-Identifier: BSD-2-Clause
// End-to-end tests: build a synthetic client folder, run the converter, read the outputs back.
#include "Atlas.h"
#include "Converter.h"
#include "Fixture.h"
#include "UoMap.h"

#include "uo/assets/Art.h"
#include "uo/assets/Color.h"
#include "uo/assets/Gumps.h"
#include "uo/assets/Lights.h"
#include "uo/assets/Texmaps.h"
#include "uo/io/Compression.h"

#include "doctest.h"

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#include "stb_image.h"

#include <cstring>
#include <filesystem>
#include <fstream>
#include <regex>
#include <sstream>

namespace fs = std::filesystem;
using namespace fixture;
using uotest::TempDir;

namespace
{

std::string slurp(const fs::path& p)
{
    std::ifstream f(p, std::ios::binary);
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

struct Png
{
    int w = 0, h = 0;
    std::vector<std::uint32_t> px;
    std::uint32_t at(int x, int y) const { return px[static_cast<std::size_t>(y) * w + x]; }
};

Png loadPng(const fs::path& p)
{
    Png out;
    int n = 0;
    unsigned char* d = stbi_load(p.string().c_str(), &out.w, &out.h, &n, 4);
    REQUIRE(d != nullptr);
    out.px.resize(static_cast<std::size_t>(out.w) * out.h);
    std::memcpy(out.px.data(), d, out.px.size() * 4);
    stbi_image_free(d);
    return out;
}

// "rect":[x,y,w,h] of frame `id` in an atlas index, and its sheet number.
bool findFrame(const std::string& json, std::uint32_t id, int& sheet, int rect[4])
{
    std::regex re("\"" + std::to_string(id) + "\":\\{\"sheet\":(\\d+),\"rect\":\\[(\\d+),(\\d+),(\\d+),(\\d+)\\]");
    std::smatch m;
    if (!std::regex_search(json, m, re))
        return false;
    sheet = std::stoi(m[1]);
    for (int i = 0; i < 4; ++i)
        rect[i] = std::stoi(m[i + 2]);
    return true;
}

// Every pixel of `img` equals the atlas page at `rect`.
void checkBlit(const Png& page, const int rect[4], const uo::assets::Image& img)
{
    REQUIRE(rect[2] == img.width);
    REQUIRE(rect[3] == img.height);
    for (int y = 0; y < img.height; ++y)
        for (int x = 0; x < img.width; ++x)
            REQUIRE(page.at(rect[0] + x, rect[1] + y) == img.at(x, y));
}

struct Client
{
    TempDir dir;
    TempDir out;

    std::vector<std::uint16_t> static0, static1, override5, gump0;
    Bytes land0;

    Client()
    {
        // art: land 0, static 0 (3x2 with a hole), static 1 (to be replaced by verdata).
        Indexed art;
        for (int i = 0; i < 1012; ++i)
            le16(land0, rgb15(i % 32, (i / 32) % 32, 7));
        art.add(0, land0);
        static0 = {rgb15(31, 0, 0), 0, rgb15(0, 31, 0), rgb15(1, 2, 3), rgb15(4, 5, 6), 0};
        art.add(0x4000, encodeStatic(3, 2, static0));
        static1 = {rgb15(9, 9, 9)};
        art.add(0x4001, encodeStatic(1, 1, static1));
        art.write(dir, "Art.mul", "artidx.mul");  // mixed case on purpose

        // gumps: gump 0 (4x2), plus a gump.def alias 10 -> {0} hue 5.
        Indexed gumps;
        gump0 = {rgb15(1, 1, 1), rgb15(1, 1, 1), 0, rgb15(2, 2, 2), 0, 0, rgb15(3, 3, 3), rgb15(3, 3, 3)};
        gumps.add(0, encodeGump(4, 2, gump0), (4 << 16) | 2);
        gumps.write(dir, "gumpart.mul", "gumpidx.mul");
        std::ofstream(dir.path / "gump.def") << "# comment\n10 {0} 5\n";

        // hues: one group of eight hues.
        Bytes hues;
        le32(hues, 0);
        for (int e = 0; e < 8; ++e)
        {
            for (int c = 0; c < 32; ++c)
                le16(hues, rgb15(c, e, 31 - c));
            le16(hues, 0);
            le16(hues, 31);
            uotest::ascii(hues, "hue" + std::to_string(e + 1), 20);
        }
        dir.write("hues.mul", hues);
        Bytes radar;
        for (int i = 0; i < 0x4010; ++i)
            le16(radar, static_cast<std::uint16_t>(i & 0x7FFF));
        dir.write("radarcol.mul", radar);

        // tiledata, pre-High Seas layout: 512 land groups and one static group.
        Bytes td;
        for (int g = 0; g < 512; ++g)
        {
            le32(td, 0);
            for (int t = 0; t < 32; ++t)
            {
                le32(td, g == 0 && t == 3 ? 0x40 : 0);  // land 3 impassable
                le16(td, static_cast<std::uint16_t>(g * 32 + t));
                uotest::ascii(td, g == 0 && t == 3 ? "rock" : "", 20);
            }
        }
        le32(td, 0);
        for (int t = 0; t < 32; ++t)
        {
            le32(td, t == 1 ? 0x200 : 0);
            td.push_back(t == 1 ? 5 : 0);  // weight
            td.push_back(0);
            le32(td, 0);
            le16(td, 0);
            le16(td, 0);
            le16(td, 0);
            td.push_back(t == 1 ? 3 : 0);  // height
            uotest::ascii(td, t == 1 ? "table\xe9" : "", 20);
        }
        dir.write("tiledata.mul", td);

        // texmaps: one 64x64; lights: one 2x2; multis: one with two parts; animdata: graphic 1.
        Indexed tex;
        Bytes t64;
        for (int i = 0; i < 64 * 64; ++i)
            le16(t64, static_cast<std::uint16_t>(i));
        tex.add(2, t64);
        tex.write(dir, "texmaps.mul", "texidx.mul");

        Indexed light;
        light.add(0, Bytes{0, 5, 0x20, 31}, (2 << 16) | 2);
        light.write(dir, "light.mul", "lightidx.mul");

        Indexed multi;
        Bytes mb;
        for (int k = 0; k < 2; ++k)
        {
            le16(mb, static_cast<std::uint16_t>(0x100 + k));
            le16(mb, static_cast<std::uint16_t>(k));
            le16(mb, static_cast<std::uint16_t>(-k));
            le16(mb, 0);
            le32(mb, 1);
        }
        multi.add(0, mb);
        multi.write(dir, "multi.mul", "multi.idx");

        Bytes ad;
        le32(ad, 0);
        for (int e = 0; e < 8; ++e)
        {
            for (int f = 0; f < 64; ++f)
                ad.push_back(e == 1 && f < 2 ? static_cast<std::uint8_t>(f) : 0);
            ad.push_back(0);
            ad.push_back(e == 1 ? 2 : 0);  // frame count
            ad.push_back(e == 1 ? 4 : 0);  // interval
            ad.push_back(0);
        }
        dir.write("animdata.mul", ad);

        // map4 (1448x1448, 181x181 blocks): land tile = block number, one static in block (7, 9).
        const int bw = 181, bh = 181;
        Bytes map, staidx, statics;
        for (int bx = 0; bx < bw; ++bx)
            for (int by = 0; by < bh; ++by)
            {
                le32(map, 0);
                for (int c = 0; c < 64; ++c)
                {
                    le16(map, static_cast<std::uint16_t>((bx * 7 + by + c) & 0x3FFF));
                    map.push_back(static_cast<std::uint8_t>(c - 32));
                }
                if (bx == 7 && by == 9)
                {
                    le32(staidx, static_cast<std::uint32_t>(statics.size()));
                    le32(staidx, 14);
                    le32(staidx, 0);
                    for (int k = 0; k < 2; ++k)
                    {
                        le16(statics, static_cast<std::uint16_t>(0x0EED + k));
                        statics.push_back(static_cast<std::uint8_t>(3 + k));
                        statics.push_back(5);
                        statics.push_back(static_cast<std::uint8_t>(10 * k));
                        le16(statics, static_cast<std::uint16_t>(k));
                    }
                }
                else
                {
                    le32(staidx, 0xFFFFFFFFu);
                    le32(staidx, 0);
                    le32(staidx, 0);
                }
            }
        dir.write("map4.mul", map);
        dir.write("staidx4.mul", staidx);
        dir.write("statics4.mul", statics);

        // verdata: replace static 1 with a 2x1 image.
        Bytes patch = encodeStatic(2, 1, {rgb15(7, 7, 7), rgb15(8, 8, 8)});
        Bytes vd;
        le32(vd, 1);
        le32(vd, 4);                // art
        le32(vd, 0x4001);           // block
        le32(vd, 4 + 20);           // position
        le32(vd, static_cast<std::uint32_t>(patch.size()));
        le32(vd, 0);
        vd.insert(vd.end(), patch.begin(), patch.end());
        dir.write("verdata.mul", vd);

        // The shard's own art: static 5 (new) as a loose file; Gumps/3.gump (new).
        fs::create_directories(dir.path / "Art" / "Statics");
        fs::create_directories(dir.path / "Gumps");
        override5 = {rgb15(20, 20, 20), rgb15(21, 21, 21)};
        dir.write("Art/Statics/5.art", encodeStatic(1, 2, override5));
        Bytes g3;
        le32(g3, 2);
        le32(g3, 1);
        Bytes rows = encodeGump(2, 1, {rgb15(5, 5, 5), 0});
        g3.insert(g3.end(), rows.begin(), rows.end());
        dir.write("Gumps/3.gump", g3);
    }

    int run(std::set<std::string> only = {})
    {
        uoconvert::Options o;
        o.uoDir  = dir.path.string();
        o.outDir = out.path.string();
        o.jobs   = 2;
        o.maxSize = 256;
        o.only   = std::move(only);
        return uoconvert::convert(o);
    }
};

}  // namespace

TEST_CASE("art: land and statics land in atlases pixel-exact, with verdata and overrides applied")
{
    Client c;
    REQUIRE(c.run({"art"}) == 0);
    fs::path art = c.out.path / "art";

    std::string land = slurp(art / "land.json");
    int sheet, rect[4];
    REQUIRE(findFrame(land, 0, sheet, rect));
    checkBlit(loadPng(art / ("land-00" + std::to_string(sheet) + ".png")), rect,
              uo::assets::Art::decodeLand(c.land0));

    std::string st = slurp(art / "statics.json");
    CHECK(st.find("\"count\":3") != std::string::npos);  // 0, 1 (verdata), 5 (override)

    REQUIRE(findFrame(st, 0, sheet, rect));
    Png page = loadPng(art / ("statics-00" + std::to_string(sheet) + ".png"));
    uo::assets::Image s0{3, 2, {}};
    for (auto v : c.static0)
        s0.pixels.push_back(v ? uo::assets::color16To32(v) | uo::assets::kOpaque : 0);
    checkBlit(page, rect, s0);

    REQUIRE(findFrame(st, 1, sheet, rect));
    CHECK(rect[2] == 2);  // verdata's 2x1, not the archive's 1x1
    CHECK(rect[3] == 1);

    REQUIRE(findFrame(st, 5, sheet, rect));
    CHECK(rect[2] == 1);
    CHECK(rect[3] == 2);

    std::string plist = slurp(art / "statics-000.plist");
    CHECK(plist.find("<key>static/0x0000</key>") != std::string::npos);
    CHECK(plist.find("<integer>3</integer>") != std::string::npos);
    CHECK(plist.find("<string>statics-000.png</string>") != std::string::npos);
}

TEST_CASE("gumps: archive, override and gump.def alias")
{
    Client c;
    REQUIRE(c.run({"gumps"}) == 0);
    fs::path g = c.out.path / "gumps";
    std::string idx = slurp(g / "gumps.json");
    int sheet, rect[4];
    REQUIRE(findFrame(idx, 0, sheet, rect));
    uo::assets::Image expect{4, 2, {}};
    for (auto v : c.gump0)
        expect.pixels.push_back(v ? uo::assets::color16To32(v) | uo::assets::kOpaque : 0);
    checkBlit(loadPng(g / "gumps-000.png"), rect, expect);
    REQUIRE(findFrame(idx, 3, sheet, rect));
    CHECK(idx.find("\"aliases\":{\"10\":{\"source\":0,\"hue\":5}}") != std::string::npos);
}

TEST_CASE("gumps: zlib-compressed UOP entries are inflated and carry their own size")
{
    Client c;
    std::vector<std::uint16_t> px = {rgb15(4, 4, 4), 0, 0, rgb15(6, 6, 6)};
    Bytes raw;
    le32(raw, 2);
    le32(raw, 2);
    Bytes rows = encodeGump(2, 2, px);
    raw.insert(raw.end(), rows.begin(), rows.end());
    Bytes packed = uo::io::deflate(raw);
    c.dir.write("gumpartLegacyMUL.uop",
                buildUop({{"build/gumpartlegacymul/00000004.tga", packed, static_cast<std::uint32_t>(raw.size()), 1}}));

    REQUIRE(c.run({"gumps"}) == 0);
    std::string idx = slurp(c.out.path / "gumps" / "gumps.json");
    int sheet, rect[4];
    REQUIRE(findFrame(idx, 4, sheet, rect));
    uo::assets::Image expect{2, 2, {}};
    for (auto v : px)
        expect.pixels.push_back(v ? uo::assets::color16To32(v) | uo::assets::kOpaque : 0);
    checkBlit(loadPng(c.out.path / "gumps" / "gumps-000.png"), rect, expect);
    CHECK(slurp(c.out.path / "manifest.json").find("compressedSkipped") == std::string::npos);
}

TEST_CASE("texmaps, lights, hues and data tables")
{
    Client c;
    REQUIRE(c.run({"texmaps", "lights", "hues", "tiledata", "multis", "animdata"}) == 0);
    fs::path o = c.out.path;

    int sheet, rect[4];
    std::string tex = slurp(o / "texmaps" / "texmaps.json");
    REQUIRE(findFrame(tex, 2, sheet, rect));
    Png tp = loadPng(o / "texmaps" / "texmaps-000.png");
    Bytes t64;
    for (int i = 0; i < 64 * 64; ++i)
        le16(t64, static_cast<std::uint16_t>(i));
    checkBlit(tp, rect, uo::assets::Texmaps::decode(t64));
    // Extruded: the pixel left of the texture repeats its first column.
    CHECK(tp.at(rect[0] - 1, rect[1]) == tp.at(rect[0], rect[1]));

    std::string lights = slurp(o / "lights" / "lights.json");
    REQUIRE(findFrame(lights, 0, sheet, rect));
    checkBlit(loadPng(o / "lights" / "lights-000.png"), rect, uo::assets::Lights::decode(Bytes{0, 5, 0x20, 31}, 2, 2));

    Png hues = loadPng(o / "hues" / "hues.png");
    CHECK(hues.w == 32);
    CHECK(hues.h == 8);
    CHECK(hues.at(7, 2) == (uo::assets::color16To32(rgb15(7, 2, 24)) | uo::assets::kOpaque));
    CHECK(slurp(o / "hues" / "hues.json").find("[\"hue3\",0,31]") != std::string::npos);
    CHECK(fs::exists(o / "hues" / "radarcol.png"));

    std::string td = slurp(o / "data" / "tiledata.json");
    CHECK(td.find("[64,3,\"rock\"]") != std::string::npos);
    CHECK(td.find("[512,5,0,0,0,0,0,3,\"table\xc3\xa9\"]") != std::string::npos);  // Latin-1 -> UTF-8

    CHECK(slurp(o / "data" / "multis.json").find("\"0\":[[256,0,0,0,1],[257,1,-1,0,1]]") != std::string::npos);
    CHECK(slurp(o / "data" / "animdata.json").find("\"1\":[[0,1],4,0]") != std::string::npos);
}

TEST_CASE("maps: chunked .uomap round-trips land, z and statics")
{
    Client c;
    REQUIRE(c.run({"maps"}) == 0);
    uoconvert::UoMapReader r;
    REQUIRE(r.open((c.out.path / "maps" / "map4.uomap").string()));
    CHECK(r.header().width == 1448);
    CHECK(r.header().chunksX == 23);  // 1448 / 64 rounded up

    // Tile (100, 77): block (12, 9), cell (4, 5) -> chunk (1, 1), local (36, 13).
    uoconvert::UoMapChunk chunk;
    REQUIRE(r.read(1, 1, chunk));
    int cell = 5 * 8 + 4;
    CHECK(chunk.land[13 * 64 + 36] == ((12 * 7 + 9 + cell) & 0x3FFF));
    CHECK(chunk.z[13 * 64 + 36] == cell - 32);

    // Statics of block (7, 9) are in chunk (0, 1) at local (59, 13) and (60, 13).
    REQUIRE(r.read(0, 1, chunk));
    REQUIRE(chunk.statics.size() == 2);
    CHECK(chunk.statics[0].x == 59);
    CHECK(chunk.statics[0].y == 13);
    CHECK(chunk.statics[0].graphic == 0x0EED);
    CHECK(chunk.statics[1].z == 10);
    CHECK(chunk.statics[1].hue == 1);

    // The last, partial chunk column still reads.
    REQUIRE(r.read(22, 22, chunk));
    Png radar = loadPng(c.out.path / "maps" / "map4-radar.png");
    CHECK(radar.w == 1448);
}

TEST_CASE("the converter refuses to write inside the UO folder")
{
    Client c;
    uoconvert::Options o;
    o.uoDir  = c.dir.path.string();
    o.outDir = (c.dir.path / "converted").string();
    CHECK(uoconvert::convert(o) == 2);
    CHECK(!fs::exists(c.dir.path / "converted"));
}

TEST_CASE("atlas planning never overlaps and respects the page size")
{
    std::vector<uoconvert::Sprite> sprites;
    std::uint32_t seed = 7;
    for (std::uint32_t i = 0; i < 600; ++i)
    {
        seed = seed * 1103515245u + 12345u;
        sprites.push_back({i, 1 + static_cast<int>(seed % 90), 1 + static_cast<int>((seed >> 8) % 120), "s"});
    }
    sprites.push_back({600, 700, 20, "wide"});  // wider than a page
    uoconvert::AtlasOptions o;
    o.maxSize = 512;
    auto pages = uoconvert::planPages(sprites, o);
    std::size_t placed = 0;
    for (const auto& p : pages)
    {
        CHECK(p.w % 4 == 0);
        CHECK(p.h % 4 == 0);
        for (std::size_t a = 0; a < p.placements.size(); ++a)
        {
            const auto& pa = p.placements[a];
            const auto& sa = sprites[pa.sprite];
            REQUIRE(pa.x + sa.w <= p.w);
            REQUIRE(pa.y + sa.h <= p.h);
            for (std::size_t b = a + 1; b < p.placements.size(); ++b)
            {
                const auto& pb = p.placements[b];
                const auto& sb = sprites[pb.sprite];
                bool apart = pa.x + sa.w <= pb.x || pb.x + sb.w <= pa.x || pa.y + sa.h <= pb.y || pb.y + sb.h <= pa.y;
                REQUIRE(apart);
            }
        }
        placed += p.placements.size();
    }
    CHECK(placed == sprites.size());
}
