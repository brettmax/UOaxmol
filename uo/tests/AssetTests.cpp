// SPDX-License-Identifier: BSD-2-Clause
#include "TestUtil.h"
#include "doctest.h"

#include "uo/assets/Art.h"
#include "uo/assets/Cliloc.h"
#include "uo/assets/Color.h"
#include "uo/assets/Gumps.h"
#include "uo/assets/Hues.h"
#include "uo/assets/Map.h"
#include "uo/assets/TileData.h"

using namespace uo::assets;
using namespace uotest;

TEST_CASE("15-bit colours expand to RGBA8 byte order")
{
    CHECK(color16To32(0x7C00) == 0x0000FF);  // pure red lands in the low byte
    CHECK(color16To32(0x03E0) == 0x00FF00);
    CHECK(color16To32(0x001F) == 0xFF0000);
    CHECK(color16To32(0x7FFF) == 0xFFFFFF);
}

TEST_CASE("land art decodes into a 44x44 diamond")
{
    Bytes raw;
    for (int i = 0; i < 1012; ++i)
        le16(raw, 0x7C00);
    Image img = Art::decodeLand(raw);
    REQUIRE(img.width == 44);
    CHECK(img.at(21, 0) == (0x0000FF | kOpaque));  // tip of the diamond
    CHECK(img.at(0, 0) == 0);                      // corners stay transparent
    CHECK(img.at(0, 21) == (0x0000FF | kOpaque));
    CHECK(img.at(22, 43) == (0x0000FF | kOpaque));
    CHECK(img.at(0, 43) == 0);
}

TEST_CASE("static art run-length rows decode")
{
    // 3x2 image. Row 0: skip 1, run of 2 red. Row 1: run of 1 blue.
    Bytes rows;
    le16(rows, 0);  // row 0 offset (u16 units after the table)
    le16(rows, 6);  // row 1 offset
    le16(rows, 1), le16(rows, 2), le16(rows, 0x7C00), le16(rows, 0x7C00), le16(rows, 0), le16(rows, 0);
    le16(rows, 0), le16(rows, 1), le16(rows, 0x001F), le16(rows, 0), le16(rows, 0);

    Bytes raw;
    le32(raw, 0);
    le16(raw, 3);
    le16(raw, 2);
    raw.insert(raw.end(), rows.begin(), rows.end());

    Image img = Art::decodeStatic(raw);
    REQUIRE(img.width == 3);
    REQUIRE(img.height == 2);
    CHECK(img.at(0, 0) == 0);
    CHECK(img.at(1, 0) == (0x0000FF | kOpaque));
    CHECK(img.at(2, 0) == (0x0000FF | kOpaque));
    CHECK(img.at(0, 1) == (0xFF0000 | kOpaque));
    CHECK(img.at(1, 1) == 0);
}

TEST_CASE("gump rows decode and can be hued")
{
    // 2x2: row 0 = 2 x white, row 1 = 1 transparent + 1 white.
    Bytes raw;
    le32(raw, 2);  // row 0 at word 2
    le32(raw, 3);  // row 1 at word 3
    le16(raw, 0x7FFF), le16(raw, 2);
    le16(raw, 0), le16(raw, 1), le16(raw, 0x7FFF), le16(raw, 1);

    Image img = Gumps::decode(raw, 2, 2);
    CHECK(img.at(0, 0) == (0xFFFFFF | kOpaque));
    CHECK(img.at(1, 0) == (0xFFFFFF | kOpaque));
    CHECK(img.at(0, 1) == 0);
    CHECK(img.at(1, 1) == (0xFFFFFF | kOpaque));

    // A hue whose ramp is all pure green.
    Bytes hues;
    le32(hues, 0);
    for (int e = 0; e < 8; ++e)
    {
        for (int c = 0; c < 32; ++c)
            le16(hues, 0x03E0);
        le16(hues, 0), le16(hues, 0);
        ascii(hues, "green", 20);
    }
    Hues h;
    h.loadFromBytes(hues.data(), hues.size(), nullptr, 0);
    REQUIRE(h.count() == 8);
    CHECK(h.entry(1)->name == "green");
    CHECK(h.entry(0) == nullptr);

    Image tinted = Gumps::decode(raw, 2, 2, &h, 1);
    CHECK(tinted.at(0, 0) == (0x00FF00 | kOpaque));
    CHECK(h.applyPartialHue(0x7C00, 1) == 0x0000FF);  // coloured pixels survive partial hues
    CHECK(h.applyPartialHue(0x4210, 1) == 0x00FF00);  // grey pixels take the hue
}

TEST_CASE("tiledata parses the pre-High Seas layout")
{
    Bytes f;
    for (int g = 0; g < 512; ++g)
    {
        le32(f, 0);
        for (int j = 0; j < 32; ++j)
        {
            le32(f, (g == 0 && j == 3) ? 0x40 : 0);  // land 3 impassable
            le16(f, g == 0 && j == 3 ? 0x1234 : 0);
            ascii(f, g == 0 && j == 3 ? "rock" : "", 20);
        }
    }
    for (int g = 0; g < 2; ++g)
    {
        le32(f, 0);
        for (int j = 0; j < 32; ++j)
        {
            bool mark = g == 1 && j == 0;
            le32(f, mark ? 0x200 : 0);  // surface
            f.push_back(mark ? 5 : 0);  // weight
            f.push_back(mark ? 2 : 0);  // layer
            le32(f, 0);
            le16(f, 0), le16(f, 0), le16(f, 0);
            f.push_back(mark ? 3 : 0);  // height
            ascii(f, mark ? "table" : "", 20);
        }
    }

    CHECK(TileData::detect(f.size()) == TileData::Format::Old);
    // A 7.0.9+ client version only breaks ties: Second Age files keep the old layout under it.
    CHECK(TileData::detect(f.size(), TileData::Format::New) == TileData::Format::Old);
    CHECK(TileData::detect(0, TileData::Format::New) == TileData::Format::New);
    CHECK(TileData::detect(0) == TileData::Format::Old);
    // The real files: Second Age (0x2000 statics) and 7.0.9+ (0x10000 statics, 64-bit flags).
    CHECK(TileData::detect(1036288, TileData::Format::New) == TileData::Format::Old);
    CHECK(TileData::detect(3188736) == TileData::Format::New);
    TileData t;
    REQUIRE(t.loadFromBytes(f));
    REQUIRE(t.land().size() == 512 * 32);
    REQUIRE(t.statics().size() == 64);
    CHECK(t.landTile(3)->is(TF_Impassable));
    CHECK(t.landTile(3)->texId == 0x1234);
    CHECK(t.landTile(3)->name == "rock");
    const StaticTile* table = t.staticTile(32);
    CHECK(table->name == "table");
    CHECK(table->is(TF_Surface));
    CHECK(table->height == 3);
    CHECK(table->weight == 5);
}

TEST_CASE("map facets read land blocks and statics")
{
    TempDir dir;
    const int w = 16, h = 16;  // 2x2 blocks
    Bytes map;
    for (int bx = 0; bx < 2; ++bx)
        for (int by = 0; by < 2; ++by)
        {
            le32(map, 0);
            for (int c = 0; c < 64; ++c)
            {
                le16(map, static_cast<std::uint16_t>(bx * 100 + by * 10 + (c == 9 ? 1 : 0)));
                map.push_back(static_cast<std::uint8_t>(c == 9 ? -5 : 0));
            }
        }

    Bytes statics;
    le16(statics, 0x0EED), statics.push_back(1), statics.push_back(2), statics.push_back(7), le16(statics, 0x21);
    Bytes staidx;
    for (int b = 0; b < 4; ++b)
    {
        bool has = b == 3;  // block (1,1)
        le32(staidx, has ? 0 : 0xFFFFFFFF), le32(staidx, has ? 7 : 0), le32(staidx, 0);
    }

    MapFacet m;
    REQUIRE(m.loadFiles(dir.write("map0.mul", map), "", dir.write("staidx0.mul", staidx),
                        dir.write("statics0.mul", statics), w, h));
    CHECK(m.land(1, 1).tileId == 1);  // cell 9 = (1,1) in block (0,0)
    CHECK(m.land(1, 1).z == -5);
    CHECK(m.land(8, 0).tileId == 100);
    CHECK(m.land(0, 8).tileId == 10);
    CHECK(m.land(99, 99).tileId == 0);
    CHECK(m.staticsBlock(0, 0).empty());
    auto s = m.staticsBlock(1, 1);
    REQUIRE(s.size() == 1);
    CHECK(s[0].graphic == 0x0EED);
    CHECK(s[0].x == 1);
    CHECK(s[0].y == 2);
    CHECK(s[0].z == 7);
    CHECK(s[0].hue == 0x21);
}

TEST_CASE("cliloc entries load and format arguments")
{
    Bytes f;
    le32(f, 2), le16(f, 1);
    auto add = [&](std::int32_t n, const std::string& s) {
        le32(f, static_cast<std::uint32_t>(n));
        f.push_back(0);
        le16(f, static_cast<std::uint16_t>(s.size()));
        f.insert(f.end(), s.begin(), s.end());
    };
    add(500000, "You see: ~1_NAME~ with ~2_ITEM~");
    add(1000, "a sword");

    Cliloc c;
    REQUIRE(c.loadFromBytes(f));
    CHECK(*c.get(1000) == "a sword");
    CHECK(c.format(500000, "Brett\t#1000") == "You see: Brett with a sword");
    CHECK(c.get(1) == nullptr);
}
