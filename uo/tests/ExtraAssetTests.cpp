// SPDX-License-Identifier: BSD-2-Clause
// Loaders added for the offline converter: texmaps, lights, multis, animdata, verdata.
#include "TestUtil.h"

#include "uo/assets/AnimData.h"
#include "uo/assets/Color.h"
#include "uo/assets/Lights.h"
#include "uo/assets/Multis.h"
#include "uo/assets/Texmaps.h"
#include "uo/io/UOFile.h"
#include "uo/io/Verdata.h"

#include "doctest.h"

using namespace uotest;
using namespace uo::assets;

TEST_CASE("texmaps: 0x2000 bytes is 64x64, anything else 128x128, all opaque")
{
    Bytes small;
    for (int i = 0; i < 64 * 64; ++i)
        le16(small, 0);
    Image a = Texmaps::decode(small);
    CHECK(a.width == 64);
    CHECK(a.at(0, 0) == kOpaque);  // colour 0 is black here, not transparent

    Bytes big;
    for (int i = 0; i < 128 * 128; ++i)
        le16(big, static_cast<std::uint16_t>(i == 5 ? 0x7C00 : 0));
    Image b = Texmaps::decode(big);
    CHECK(b.width == 128);
    CHECK(b.at(5, 0) == (color16To32(0x7C00) | kOpaque));
    CHECK(Texmaps::decode(Bytes(100, 0)).empty());
}

TEST_CASE("lights: negative values are bit-inverted and zero is transparent")
{
    Image img = Lights::decode(Bytes{0, 5, 0x20, 31}, 2, 2);
    CHECK(img.at(0, 0) == 0);
    CHECK(img.at(1, 0) == ((5u << 19) | (5u << 11) | (5u << 3) | kOpaque));
    CHECK(img.at(0, 1) == ((31u << 19) | (31u << 11) | (31u << 3) | kOpaque));  // ~0x20 & 0x1F
    CHECK(Lights::decode(Bytes{1, 2}, 2, 2).empty());
}

TEST_CASE("multis: 12-byte records before 7.0.9, 16 after")
{
    Bytes b;
    le16(b, 0x64);
    le16(b, static_cast<std::uint16_t>(-2));
    le16(b, 3);
    le16(b, 7);
    le32(b, 0);
    auto parts = Multis::decode(b, false);
    REQUIRE(parts.size() == 1);
    CHECK(parts[0].graphic == 0x64);
    CHECK(parts[0].x == -2);
    CHECK(parts[0].z == 7);
    CHECK_FALSE(parts[0].visible);

    le32(b, 0);  // 16 bytes: one new-format record
    CHECK(Multis::decode(b, true).size() == 1);
}

TEST_CASE("multi loader: reads multi.mul entries by id, cached, with their footprint")
{
    TempDir dir;
    Bytes data;
    auto part = [&](std::uint16_t g, std::int16_t x, std::int16_t y, std::int16_t z, std::uint32_t flags) {
        le16(data, g);
        le16(data, static_cast<std::uint16_t>(x));
        le16(data, static_cast<std::uint16_t>(y));
        le16(data, static_cast<std::uint16_t>(z));
        le32(data, flags);
    };
    // Entry 1: a boat-like multi whose first part is invisible (the item's own graphic).
    part(0x3E4E, 0, 0, 0, 0);
    part(0x3E65, -1, 2, 0, 1);
    part(0x3E66, 3, -4, 5, 1);

    Bytes idx;
    le32(idx, 0xFFFFFFFF), le32(idx, 0), le32(idx, 0);  // entry 0: absent
    le32(idx, 0), le32(idx, static_cast<std::uint32_t>(data.size())), le32(idx, 0);

    auto file = std::make_unique<uo::io::MulFile>(dir.write("multi.mul", data), dir.write("multi.idx", idx));
    REQUIRE(file->load());
    MultiLoader multis(std::move(file), false);

    CHECK(multis.count() == 2);
    CHECK(multis.components(0).empty());
    CHECK(multis.components(7).empty());

    const auto& parts = multis.components(1);
    REQUIRE(parts.size() == 3);
    CHECK_FALSE(parts[0].visible);
    CHECK(parts[1].visible);
    CHECK(parts[2].z == 5);
    CHECK(&multis.components(1) == &parts);

    const MultiExtent e = multis.extent(1);
    CHECK(e.minX == -1);
    CHECK(e.minY == -4);
    CHECK(e.maxX == 3);
    CHECK(e.maxY == 2);
}

TEST_CASE("animdata: entry g is at g * 68 + 4 * (g / 8 + 1)")
{
    Bytes b;
    for (int group = 0; group < 2; ++group)
    {
        le32(b, 0);
        for (int e = 0; e < 8; ++e)
        {
            int g = group * 8 + e;
            for (int f = 0; f < 64; ++f)
                b.push_back(g == 9 && f < 3 ? static_cast<std::uint8_t>(f * 2) : 0);
            b.push_back(0);
            b.push_back(g == 9 ? 3 : 0);
            b.push_back(g == 9 ? 6 : 0);
            b.push_back(g == 9 ? 1 : 0);
        }
    }
    AnimData ad;
    ad.loadFromBytes(b);
    CHECK(ad.count() == 16);
    const AnimDataEntry* e = ad.get(9);
    REQUIRE(e);
    CHECK(e->frameCount == 3);
    CHECK(e->frames[2] == 4);
    CHECK(e->frameInterval == 6);
    CHECK(e->frameStart == 1);
    CHECK(ad.get(16) == nullptr);
}

TEST_CASE("verdata: patches are found by (file, block), the last one winning")
{
    TempDir dir;
    Bytes b;
    le32(b, 2);
    for (std::uint32_t pos : {44u, 46u})
    {
        le32(b, uo::io::Verdata::Art);
        le32(b, 0x4001);
        le32(b, pos);
        le32(b, 2);
        le32(b, 0);
    }
    le16(b, 0x1111);
    le16(b, 0x2222);
    uo::io::Verdata v;
    REQUIRE(v.load(dir.write("verdata.mul", b)));
    CHECK(v.patches().size() == 2);
    const auto* p = v.find(uo::io::Verdata::Art, 0x4001);
    REQUIRE(p);
    CHECK(v.bytes(*p)[0] == 0x22);
    CHECK(v.find(uo::io::Verdata::Gumps, 0x4001) == nullptr);
}
