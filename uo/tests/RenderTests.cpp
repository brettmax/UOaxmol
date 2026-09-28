// SPDX-License-Identifier: BSD-2-Clause
//
// Unit tests for the engine-free world renderer (uo/client/world). Expected
// values come from ClassicUO's behaviour for the same inputs.

#include "doctest.h"

#include <cmath>
#include <functional>
#include <map>
#include <string>
#include <vector>

#include "uo/assets/Texmaps.h"
#include "uo/render/HueTexture.h"
#include "uo/render/HueVector.h"
#include "uo/render/LandStretch.h"
#include "uo/render/Pick.h"
#include "uo/render/WorldGeometry.h"
#include "uo/render/WorldMap.h"

using namespace uo;
using namespace uo::render;

namespace
{

struct FakeMap : IMapSource
{
    int w, h;
    std::vector<LandCell> cells;
    std::map<std::pair<int, int>, std::vector<StaticEntry>> blockStatics;

    FakeMap(int w_, int h_, uint16_t graphic = 3) : w(w_), h(h_), cells(static_cast<size_t>(w_ * h_), LandCell{graphic, 0}) {}

    int width() const override { return w; }
    int height() const override { return h; }

    bool land(int x, int y, LandCell& out) const override
    {
        if (x < 0 || y < 0 || x >= w || y >= h)
        {
            return false;
        }
        out = cells[static_cast<size_t>(y * w + x)];
        return true;
    }

    void statics(int bx, int by, std::vector<StaticEntry>& out) const override
    {
        auto it = blockStatics.find({bx, by});
        if (it != blockStatics.end())
        {
            out.insert(out.end(), it->second.begin(), it->second.end());
        }
    }

    void setZ(int x, int y, int8_t z) { cells[static_cast<size_t>(y * w + x)].z = z; }

    void addStatic(int x, int y, int8_t z, uint16_t graphic, uint16_t hue = 0)
    {
        blockStatics[{x / 8, y / 8}].push_back(
            {graphic, static_cast<uint8_t>(x % 8), static_cast<uint8_t>(y % 8), z, hue});
    }
};

struct FakeTiles : ITileData
{
    std::map<uint16_t, StaticTileData> items;
    std::map<uint16_t, LandTileData> lands;
    bool texmaps = true;

    LandTileData land(uint16_t g) const override
    {
        auto it = lands.find(g);
        return it != lands.end() ? it->second : LandTileData{0, 1};
    }

    StaticTileData item(uint16_t g) const override
    {
        auto it = items.find(g);
        return it != items.end() ? it->second : StaticTileData{};
    }

    int itemCount() const override { return 0x10000; }
    bool hasTexmap(uint16_t) const override { return texmaps; }
};

// Every art entry comes from one of two fake textures so batching is visible.
struct FakeTextures : ITextureSource
{
    int landTex = 1, texmapTex = 2, itemTex = 3;
    std::map<uint16_t, std::pair<int, int>> itemSizes;  // graphic -> w, h

    bool landArt(uint16_t, TextureRegion& r) override
    {
        r = {&landTex, 256, 256, 0, 0, 44, 44};
        return true;
    }

    bool texmap(uint16_t, TextureRegion& r) override
    {
        r = {&texmapTex, 64, 64, 0, 0, 64, 64};
        return true;
    }

    bool itemArt(uint16_t g, TextureRegion& r) override
    {
        auto it = itemSizes.find(g);
        auto [w, h] = it != itemSizes.end() ? it->second : std::pair{44, 44};
        r = {&itemTex, 512, 512, 0, 0, w, h};
        return true;
    }
};

}  // namespace

TEST_CASE("hue vectors match ClassicUO ShaderHueTranslator")
{
    CHECK((makeHueVector(0) == HueVector{0, SHADER_NONE, 1}));
    CHECK((makeHueVector(0x21) == HueVector{0x20, SHADER_HUED, 1}));
    CHECK((makeHueVector(0x8021, false, 1) == HueVector{0x20, SHADER_PARTIAL_HUED, 1}));
    CHECK((makeHueVector(0x21, true, 1) == HueVector{0x20, SHADER_PARTIAL_HUED, 1}));
    CHECK((makeHueVector(0x8000, false, 1).mode == float(SHADER_NONE)));  // partial with no hue is no hue
    CHECK((makeHueVector(0x4001, false, 1).mode == float(SHADER_SPECTRAL)));
    CHECK((makeHueVector(0x21, false, 0.5f, false, false, true).alpha == 1.5f));
    CHECK((makeHueVector(0x21, false, 1, true).mode == float(SHADER_HUED + kGumpShaderOffset)));
    CHECK((makeHueVector(0x21, false, 1, false, true).mode == float(SHADER_EFFECT_HUED)));
    CHECK((makeHueVector(0x21, true, 1, false, true).mode == float(SHADER_PARTIAL_HUED)));

    CHECK((makeLandHueVector(0, false) == HueVector{0, SHADER_NONE, 1}));
    CHECK((makeLandHueVector(0, true) == HueVector{0, SHADER_LAND, 1}));
    CHECK((makeLandHueVector(5, true) == HueVector{4, SHADER_LAND_HUED, 1}));
    CHECK((makeLandHueVector(5, false) == HueVector{4, SHADER_HUED, 1}));
}

TEST_CASE("hue texture packing and texmap decoding")
{
    // 20 hues of 32 texels, as Hues::buildHueTexture lays them out.
    std::vector<std::uint32_t> ramps(20 * kHueRampWidth, 0xFF000000u);
    ramps[17 * kHueRampWidth]      = 0xFF0000FFu;
    ramps[17 * kHueRampWidth + 31] = 0xFFFF0000u;

    auto px = packHueTexture(ramps);
    CHECK(px.size() == static_cast<size_t>(kHueTextureWidth * kHueTextureHeight));

    // Hue index 17 (hue 18) -> row 1, column 1, where uo_world_fs looks for it.
    const size_t base = 1 * kHueTextureWidth + 1 * kHueRampWidth;
    CHECK(px[base] == 0xFF0000FFu);
    CHECK(px[base + 31] == 0xFFFF0000u);
    CHECK(px[20 * kHueRampWidth] == 0u);  // padding past the last hue

    // Oversized input is truncated, not overflowed.
    std::vector<std::uint32_t> huge(px.size() + 100, 1u);
    CHECK(packHueTexture(huge).size() == px.size());

    // 64x64 texmap: 0x2000 bytes of 16-bit colour, all opaque.
    std::vector<std::uint8_t> raw(0x2000, 0);
    raw[0] = 0x00;
    raw[1] = 0x7C;  // first pixel pure red
    auto img = assets::Texmaps::decode(raw);
    CHECK(img.width == 64);
    CHECK(img.height == 64);
    CHECK(img.at(0, 0) == 0xFF0000FFu);
    CHECK(img.at(1, 0) == 0xFF000000u);

    // Anything else is 128x128; short data is rejected.
    CHECK(assets::Texmaps::decode(std::vector<std::uint8_t>(0x8000, 0)).width == 128);
    CHECK(assets::Texmaps::decode(std::vector<std::uint8_t>(100, 0)).empty());
}

TEST_CASE("land stretch offsets, average Z and normals")
{
    FakeMap map(16, 16);
    FakeTiles tiles;

    auto flat = computeLandStretch(map, 4, 4, 0, true);
    CHECK(!flat.stretched);
    CHECK(flat.averageZ == 0);

    // Slope: (5,4) is raised, so tile (4,4)'s right corner lifts.
    map.setZ(5, 4, 8);
    auto slope = computeLandStretch(map, 4, 4, 0, true);
    CHECK(slope.stretched);
    CHECK(slope.offsets.right == 32);
    CHECK(slope.offsets.top == 0);
    CHECK(slope.minZ == 0);
    // |top - bottom| = 0 <= |left - right| = 8, so average top and bottom.
    CHECK(slope.averageZ == 0);

    const Vec3& n = slope.normalRight;
    CHECK(std::sqrt(n.x * n.x + n.y * n.y + n.z * n.z) == doctest::Approx(1.0f).epsilon(1e-5f));
    CHECK(slope.normalTop.z < 1.0f);

    auto noStretch = computeLandStretch(map, 4, 4, 0, false);
    CHECK(!noStretch.stretched);

    // Off-map neighbours read as -125 like ClassicUO.
    CHECK(map.landZ(-1, 0) == kInvalidLandZ);
    CHECK(map.landZ(16, 0) == kInvalidLandZ);

    CHECK(!canStretchLand({assets::TF_Wet, 0}, tiles));
    CHECK(canStretchLand({0, 0}, tiles));
    tiles.texmaps = false;
    CHECK(!canStretchLand({0, 5}, tiles));
}

TEST_CASE("priority Z and tile list order match Chunk.AddGameObject")
{
    FakeTiles tiles;
    tiles.items[0x100] = {assets::TF_Background, 0};
    tiles.items[0x200] = {0, 10};
    tiles.items[0x300] = {0, 0};

    WorldObject land;
    land.kind = ObjectKind::Land;
    land.z    = 0;
    CHECK(computePriority(land, tiles).priorityZ == -2);
    CHECK(computePriority(land, tiles).state == 0);

    land.land.stretched = true;
    land.land.averageZ  = 6;
    CHECK(computePriority(land, tiles).priorityZ == 4);

    WorldObject st;
    st.graphic = 0x100;
    CHECK(computePriority(st, tiles).priorityZ == -1);
    st.graphic = 0x200;
    CHECK(computePriority(st, tiles).priorityZ == 1);

    WorldObject mob;
    mob.kind = ObjectKind::Mobile;
    CHECK(computePriority(mob, tiles).priorityZ == 1);

    WorldObject multi;
    multi.kind       = ObjectKind::Multi;
    multi.graphic    = 0x300;
    multi.multiState = MULTI_GENERIC_INTERNAL;
    CHECK(computePriority(multi, tiles).priorityZ == -1);
    CHECK(computePriority(multi, tiles).state == 1);
    multi.multiState = MULTI_PREVIEW;
    CHECK(computePriority(multi, tiles).priorityZ == 1);
    CHECK(computePriority(multi, tiles).state == 2);

    // Cell order: statics loaded in file order end up sorted by priority, land first.
    FakeMap map(8, 8);
    map.addStatic(2, 2, 0, 0x200);  // priority 1
    map.addStatic(2, 2, 0, 0x100);  // priority -1
    map.addStatic(2, 2, 0, 0x300);  // priority 0

    WorldMap world(map, tiles);
    world.loadBlock(0, 0);
    const auto* cell = world.cellAt(2, 2);
    CHECK(cell);
    CHECK(cell->size() == 4);
    CHECK((*cell)[0].kind == ObjectKind::Land);
    CHECK((*cell)[1].graphic == 0x100);
    CHECK((*cell)[2].graphic == 0x300);
    CHECK((*cell)[3].graphic == 0x200);

    // A later object with equal priority goes after existing ones...
    WorldObject item;
    item.kind    = ObjectKind::Item;
    item.graphic = 0x300;
    item.x = 2, item.y = 2, item.serial = 7;
    CHECK(world.addObject(item));
    CHECK((*cell)[3].serial == 7);

    // ...but land goes before equal-priority objects.
    WorldObject land2;
    land2.kind = ObjectKind::Land;
    land2.x = 2, land2.y = 2, land2.z = 1;  // priority -1, same as 0x100
    land2.serial = 9;
    CHECK(world.addObject(land2));
    CHECK((*cell)[1].serial == 9);

    WorldObject far;
    far.x = 100, far.y = 100;
    CHECK(!world.addObject(far));

    CHECK(world.removeObject(7, 2, 2));
    CHECK(cell->size() == 5);
    CHECK(!world.removeObject(7, 2, 2));
    CHECK(!world.removeObject(9, 3, 3));
    CHECK(!world.removeObject(9, 100, 100));
}

TEST_CASE("static visibility matches GameObject.CanBeDrawn")
{
    CHECK(canDrawStatic(0x0E75, {}));
    CHECK(!canDrawStatic(0x0001, {}));
    CHECK(!canDrawStatic(0x21BC, {}));
    CHECK(!canDrawStatic(0x219A, {}));
    CHECK(!canDrawStatic(0x63D3, {}));
    CHECK(!canDrawStatic(0x0E75, {assets::TF_NoDiagonal, 0}));
    CHECK(!canDrawStatic(0x0E75, {0, 0, "NoDraw tile"}));
    CHECK(!canDrawStatic(0x9E4C, {assets::TF_Surface, 0}));
    CHECK(canDrawStatic(0x9E4C, {}));
}

TEST_CASE("draw list is back-to-front and honours view filters")
{
    FakeMap map(16, 16);
    FakeTiles tiles;
    tiles.items[0x200] = {0, 10};
    tiles.items[0x400] = {assets::TF_Roof, 3};
    tiles.items[0x500] = {assets::TF_Foliage | assets::TF_PartialHue, 20};

    map.addStatic(0, 0, 10, 0x200);
    map.addStatic(3, 3, 40, 0x400);
    map.addStatic(5, 2, 0, 0x500, 0x22);

    WorldMap world(map, tiles);

    ViewParams view;
    view.maxTileX = 7;
    view.maxTileY = 7;
    world.ensureLoaded(view);

    std::vector<DrawItem> list;
    world.buildDrawList(view, list);

    CHECK(list.size() == 64 + 3);

    for (size_t i = 1; i < list.size(); ++i)
    {
        CHECK(list[i - 1].depth <= list[i].depth);
    }

    // The tall static at (0,0) z10 sorts above its land but below land at (1,0).
    auto indexOf = [&](const std::function<bool(const DrawItem&)>& pred) {
        for (size_t i = 0; i < list.size(); ++i)
        {
            if (pred(list[i]))
            {
                return static_cast<int>(i);
            }
        }
        return -1;
    };

    const WorldObject* land00 = &world.cellAt(0, 0)->front();
    const WorldObject* land10 = &world.cellAt(1, 0)->front();
    int tall                  = indexOf([](const DrawItem& d) { return d.graphic == 0x200; });
    int l00                   = indexOf([&](const DrawItem& d) { return d.object == land00; });
    int l10                   = indexOf([&](const DrawItem& d) { return d.object == land10; });
    CHECK(l00 < tall);
    CHECK(tall < l10);

    // Screen positions: tile (0,0) z10 static.
    const DrawItem& t = list[static_cast<size_t>(tall)];
    CHECK(t.screenX == -22);
    CHECK(t.screenY == -22 - 40);
    CHECK(t.type == DrawType::Static);

    // Partial hue flag reaches the hue vector.
    int fol = indexOf([](const DrawItem& d) { return d.graphic == 0x500 && d.type == DrawType::Static; });
    CHECK(fol >= 0);
    CHECK(list[static_cast<size_t>(fol)].hue.mode == float(SHADER_PARTIAL_HUED));
    CHECK(fol >= 0);
    CHECK(list[static_cast<size_t>(fol)].hue.hue == 0x21);

    // Roof hidden by maxZ and by hideRoofs.
    view.maxZ = 30;
    world.buildDrawList(view, list);
    CHECK(indexOf([](const DrawItem& d) { return d.graphic == 0x400; }) < 0);

    view.maxZ      = 127;
    view.hideRoofs = true;
    world.buildDrawList(view, list);
    CHECK(indexOf([](const DrawItem& d) { return d.graphic == 0x400; }) < 0);

    // maxGroundZ stops a tile's list at the first object above it.
    view.hideRoofs  = false;
    view.maxGroundZ = 5;
    world.buildDrawList(view, list);
    CHECK(indexOf([](const DrawItem& d) { return d.graphic == 0x200; }) < 0);
    CHECK(indexOf([&](const DrawItem& d) { return d.object == land00; }) >= 0);

    // Shadows under foliage sit just behind the static.
    view.maxGroundZ    = 127;
    view.staticShadows = true;
    world.buildDrawList(view, list);
    int sh = indexOf([](const DrawItem& d) { return d.type == DrawType::Shadow; });
    fol    = indexOf([](const DrawItem& d) { return d.graphic == 0x500 && d.type == DrawType::Static; });
    CHECK(sh >= 0);
    CHECK(sh < fol);
    CHECK(list[static_cast<size_t>(sh)].hue.mode == float(SHADER_SHADOW));

    // Override hue drops partial hueing.
    view.overrideHue = 0x386;
    world.buildDrawList(view, list);
    fol = indexOf([](const DrawItem& d) { return d.graphic == 0x500 && d.type == DrawType::Static; });
    CHECK(list[static_cast<size_t>(fol)].hue.mode == float(SHADER_HUED));
    CHECK(list[static_cast<size_t>(fol)].hue.hue == 0x385);
}

TEST_CASE("circle of transparency marks eligible statics")
{
    FakeMap map(8, 8);
    FakeTiles tiles;
    tiles.items[0x600] = {assets::TF_Wall, 20};
    tiles.items[0x700] = {0, 2};  // small, not transparent-capable
    map.addStatic(1, 1, 10, 0x600);
    map.addStatic(2, 2, 10, 0x700);

    WorldMap world(map, tiles);
    ViewParams view;
    view.maxTileX = 7, view.maxTileY = 7;
    view.circleOfTransparency = true;
    view.playerZ              = 0;
    world.ensureLoaded(view);

    std::vector<DrawItem> list;
    world.buildDrawList(view, list);

    for (const DrawItem& d : list)
    {
        if (d.graphic == 0x600)
        {
            CHECK(d.hue.alpha > 1.0f);
        }
        if (d.graphic == 0x700)
        {
            CHECK(d.hue.alpha <= 1.0f);
        }
    }
}

TEST_CASE("trees stay visible inside the circle of transparency")
{
    CHECK(isTree(0x0CCA, StaticTileData{assets::TF_Impassable, 20}));
    CHECK_FALSE(isTree(0x0CCA, StaticTileData{0, 20}));  // passable: vegetation, not a tree
    CHECK_FALSE(isTree(0x0CCF, StaticTileData{assets::TF_Impassable, 20}));
    CHECK(isTree(0x12BD, StaticTileData{assets::TF_Impassable, 20}));

    FakeMap map(8, 8);
    FakeTiles tiles;
    tiles.items[0x0CCA] = {assets::TF_Impassable | assets::TF_Wall, 20};  // tree
    tiles.items[0x0CCB] = {assets::TF_Wall, 20};                          // passable: not a tree
    map.addStatic(1, 1, 10, 0x0CCA);
    map.addStatic(2, 2, 10, 0x0CCB);

    WorldMap world(map, tiles);
    ViewParams view;
    view.maxTileX = 7, view.maxTileY = 7;
    view.circleOfTransparency = true;
    world.ensureLoaded(view);

    std::vector<DrawItem> list;
    world.buildDrawList(view, list);

    int seen = 0;
    for (const DrawItem& d : list)
    {
        if (d.graphic == 0x0CCA)
        {
            CHECK(d.hue.alpha <= 1.0f);
            ++seen;
        }
        if (d.graphic == 0x0CCB)
        {
            CHECK(d.hue.alpha > 1.0f);
            ++seen;
        }
    }
    CHECK(seen == 2);
}

TEST_CASE("geometry places land, stretched land and statics like Batcher2D")
{
    FakeMap map(8, 8);
    FakeTiles tiles;
    tiles.items[0x200] = {0, 10};
    map.setZ(3, 2, 4);  // makes tiles around (2,2) stretched
    map.addStatic(5, 5, 0, 0x200);

    WorldMap world(map, tiles);
    ViewParams view;
    view.maxTileX = 7, view.maxTileY = 7;
    world.ensureLoaded(view);

    std::vector<DrawItem> list;
    world.buildDrawList(view, list);

    FakeTextures tex;
    tex.itemSizes[0x200] = {60, 90};

    WorldGeometry g;
    buildWorldGeometry(list, tiles, tex, g);

    CHECK(g.vertices.size() == list.size() * 4);
    CHECK(g.indices.size() == list.size() * 6);

    uint32_t total = 0;
    for (size_t i = 0; i < g.batches.size(); ++i)
    {
        CHECK(g.batches[i].firstIndex == total);
        total += g.batches[i].indexCount;
        if (i > 0)
        {
            CHECK(g.batches[i].texture != g.batches[i - 1].texture);
        }
    }
    CHECK(total == g.indices.size());

    for (size_t i = 0; i < list.size(); ++i)
    {
        const DrawItem& d   = list[i];
        const WorldVertex* v = &g.vertices[i * 4];

        if (d.type == DrawType::LandFlat)
        {
            CHECK(v[0].x == d.screenX);
            CHECK(v[0].y == d.screenY);
            CHECK(v[3].x == d.screenX + 44);
            CHECK(v[3].y == d.screenY + 44);
            CHECK(v[0].u == doctest::Approx(0.5f / 256).epsilon(1e-6f));
            CHECK(v[3].u == doctest::Approx(43.5f / 256).epsilon(1e-6f));
        }
        else if (d.type == DrawType::LandStretched)
        {
            const YOffsets& o = d.object->land.offsets;
            CHECK(v[0].x == d.screenX + 22);
            CHECK(v[0].y == d.screenY - o.top);
            CHECK(v[1].x == d.screenX + 44);
            CHECK(v[1].y == d.screenY + 22 - o.right);
            CHECK(v[2].x == d.screenX);
            CHECK(v[2].y == d.screenY + 22 - o.left);
            CHECK(v[3].x == d.screenX + 22);
            CHECK(v[3].y == d.screenY + 44 - o.bottom);
            CHECK(v[0].mode == float(SHADER_LAND));
        }
        else if (d.type == DrawType::Static)
        {
            // 60x90 art: x -= 60/2 - 22, y -= 90 - 44.
            CHECK(v[0].x == d.screenX - 8);
            CHECK(v[0].y == d.screenY - 46);
            CHECK(v[3].x == d.screenX - 8 + 60);
            CHECK(v[3].y == d.screenY - 46 + 90);
        }
    }

    // Stretched tile (2,2): top corner at its own z, right corner raised.
    const WorldObject& land22 = world.cellAt(2, 2)->front();
    CHECK(land22.land.stretched);
    CHECK(land22.land.offsets.right == 16);
}


namespace
{

// 20x40 art whose left half is transparent.
struct FakeArt : IArtHitTest
{
    bool itemSize(uint16_t, int& w, int& h) override
    {
        w = 20;
        h = 40;
        return true;
    }

    bool itemOpaque(uint16_t, int x, int y) override { return x >= 10 && x < 20 && y >= 0 && y < 40; }
};

}  // namespace

TEST_CASE("pick returns the topmost object under the point, like SelectedObject")
{
    FakeTiles tiles;
    FakeArt art;

    WorldObject land;
    land.kind = ObjectKind::Land;
    land.x = 5;
    land.y = 5;

    WorldObject item;
    item.kind   = ObjectKind::Item;
    item.serial = 0x40000001;

    std::vector<DrawItem> list;
    list.push_back({DrawType::LandFlat, 3, 100, 100, 0, makeHueVector(0), &land});
    // Static anchored at the land tile: art origin (100 - (10 - 22), 100 - (40 - 44)) = (112, 104).
    list.push_back({DrawType::Static, 0x200, 100, 100, 1, makeHueVector(0), &item});

    SUBCASE("opaque art pixel beats the land under it")
    {
        const DrawItem* hit = pick(list, tiles, art, 125, 120);
        REQUIRE(hit);
        CHECK(hit->object->serial == 0x40000001u);
    }

    SUBCASE("transparent art pixel falls through to land")
    {
        const DrawItem* hit = pick(list, tiles, art, 115, 120);
        REQUIRE(hit);
        CHECK(hit->object->kind == ObjectKind::Land);
    }

    SUBCASE("land is a diamond, not a box")
    {
        CHECK(pick(list, tiles, art, 122, 122) != nullptr);
        CHECK(pick(list, tiles, art, 101, 101) == nullptr);
        CHECK(pick(list, tiles, art, 142, 142) == nullptr);
    }

    SUBCASE("shadows and invisible objects are never picked")
    {
        list[1].type = DrawType::Shadow;
        CHECK(pick(list, tiles, art, 125, 120)->object->kind == ObjectKind::Land);
        list[1].type      = DrawType::Static;
        list[1].hue.alpha = 0;
        CHECK(pick(list, tiles, art, 125, 120)->object->kind == ObjectKind::Land);
    }

    SUBCASE("circle of transparency hides cut statics only inside the circle")
    {
        list[1].hue = makeHueVector(0, false, 1.0f, false, false, true);
        REQUIRE(list[1].hue.alpha > 1.0f);
        CHECK(pick(list, tiles, art, 125, 120, {125, 120, 10})->object->kind == ObjectKind::Land);
        CHECK(pick(list, tiles, art, 125, 105, {125, 125, 10})->object->serial == 0x40000001u);
        CHECK(pick(list, tiles, art, 125, 120)->object->serial == 0x40000001u);
    }
}

TEST_CASE("pick follows stretched land corners")
{
    FakeTiles tiles;
    FakeArt art;

    WorldObject land;
    land.kind                = ObjectKind::Land;
    land.land.offsets.top    = 20;  // top corner raised 20 px
    land.land.offsets.right  = 0;
    land.land.offsets.left   = 0;
    land.land.offsets.bottom = 0;

    std::vector<DrawItem> list{{DrawType::LandStretched, 3, 0, 0, 0, makeLandHueVector(0, true), &land}};

    CHECK(pick(list, tiles, art, 22, -15) != nullptr);  // inside the raised top
    CHECK(pick(list, tiles, art, 22, 40) != nullptr);
    CHECK(pick(list, tiles, art, 2, -10) == nullptr);

    tiles.texmaps = false;  // falls back to flat art raised by z (0 here)
    CHECK(pick(list, tiles, art, 22, -15) == nullptr);
    CHECK(pick(list, tiles, art, 22, 22) != nullptr);
}

TEST_CASE("view Z limits follow GameScene.UpdateMaxDrawZ")
{
    constexpr uint16_t kRoof = 0x500, kFloor = 0x501, kRoofSurface = 0x502;
    FakeTiles tiles;
    tiles.items[kRoof]        = StaticTileData{assets::TF_Roof, 0};
    tiles.items[kFloor]       = StaticTileData{assets::TF_Surface, 0};
    tiles.items[kRoofSurface] = StaticTileData{assets::TF_Roof | assets::TF_Surface, 0};

    ViewParams view;
    view.maxTileX = 23;
    view.maxTileY = 23;

    SUBCASE("open ground draws everything")
    {
        FakeMap map(24, 24);
        WorldMap world(map, tiles);
        world.ensureLoaded(view);
        const ViewZ v = world.computeViewZ(10, 10, 0);
        CHECK(v.maxZ == 127);
        CHECK(v.maxGroundZ == 127);
        CHECK_FALSE(v.hideRoofs);
        CHECK(world.computeViewZ(10, 10, 0, false).hideRoofs);
    }

    SUBCASE("a roof in front cuts at the lowest connected roof tile")
    {
        FakeMap map(24, 24);
        map.addStatic(11, 11, 20, kRoof);
        map.addStatic(12, 11, 17, kRoof);  // connected, within 6 Z
        map.addStatic(13, 11, 5, kRoof);   // too far below: not part of this roof
        WorldMap world(map, tiles);
        world.ensureLoaded(view);
        const ViewZ v = world.computeViewZ(10, 10, 0);
        CHECK(v.maxZ == 17);
        CHECK(v.maxGroundZ == 17);
        CHECK(v.hideRoofs);
        CHECK(world.calculateNearZ(20, 11, 11, 20) == 17);
        CHECK(world.calculateNearZ(20, 1, 1, 20) == 20);
    }

    SUBCASE("a roof that is also a surface is walked on, not hidden")
    {
        FakeMap map(24, 24);
        map.addStatic(11, 11, 20, kRoofSurface);
        WorldMap world(map, tiles);
        world.ensureLoaded(view);
        CHECK(world.computeViewZ(10, 10, 0).maxZ == 127);
    }

    SUBCASE("an upper floor over the player hides it and roofs")
    {
        FakeMap map(24, 24);
        map.addStatic(10, 10, 20, kFloor);
        WorldMap world(map, tiles);
        world.ensureLoaded(view);
        const ViewZ v = world.computeViewZ(10, 10, 0);
        CHECK(v.maxZ == 20);
        CHECK(v.maxGroundZ == 20);
        CHECK(v.hideRoofs);
        // Standing on that floor, nothing is overhead.
        CHECK(world.computeViewZ(10, 10, 20).maxZ == 127);
    }

    SUBCASE("ground over the player (a cave) cuts at player Z + 16")
    {
        FakeMap map(24, 24);
        for (int y = 0; y < 24; ++y)
        {
            for (int x = 0; x < 24; ++x)
            {
                map.setZ(x, y, 30);
            }
        }
        WorldMap world(map, tiles);
        world.ensureLoaded(view);
        const ViewZ v = world.computeViewZ(10, 10, 0);
        CHECK(v.maxZ == 16);
        CHECK(v.maxGroundZ == 16);
    }
}
