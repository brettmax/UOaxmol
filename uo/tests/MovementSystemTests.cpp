// SPDX-License-Identifier: BSD-2-Clause
#include "uo/assets/Map.h"
#include "uo/assets/Multis.h"
#include "uo/assets/TileData.h"
#include "uo/movement/MovementSystem.h"
#include "uo/world/PacketHandlers.h"
#include "uo/world/World.h"

#include "TestUtil.h"
#include "doctest.h"

using namespace uo;
using namespace uo::movement;
using namespace uotest;

namespace
{

// 16x16 facet of flat grass (land 3) with a wall (static 1, impassable, 20 high) at (5, 4).
struct TestMap
{
    TempDir dir;
    assets::MapFacet map;
    assets::TileData tiledata;

    TestMap()
    {
        Bytes td;
        for (int g = 0; g < 512; ++g)
        {
            le32(td, 0);
            for (int j = 0; j < 32; ++j)
            {
                le32(td, 0);
                le16(td, 0);
                ascii(td, "", 20);
            }
        }
        for (int g = 0; g < 2; ++g)
        {
            le32(td, 0);
            for (int j = 0; j < 32; ++j)
            {
                const bool wall = g == 0 && j == 1;
                le32(td, wall ? 0x40 : 0);
                td.push_back(0);
                td.push_back(0);
                le32(td, 0);
                le16(td, 0), le16(td, 0), le16(td, 0);
                td.push_back(wall ? 20 : 0);
                ascii(td, wall ? "wall" : "", 20);
            }
        }
        REQUIRE(tiledata.loadFromBytes(td));

        Bytes land;
        for (int b = 0; b < 4; ++b)
        {
            le32(land, 0);
            for (int c = 0; c < 64; ++c)
            {
                le16(land, 3);
                land.push_back(0);
            }
        }

        // Block (0,0) holds the wall at cell (5,4).
        Bytes statics;
        le16(statics, 0x0001), statics.push_back(5), statics.push_back(4), statics.push_back(0), le16(statics, 0);
        Bytes staidx;
        for (int b = 0; b < 4; ++b)
        {
            const bool has = b == 0;
            le32(staidx, has ? 0 : 0xFFFFFFFF), le32(staidx, has ? 7 : 0), le32(staidx, 0);
        }

        REQUIRE(map.loadFiles(dir.write("map0.mul", land), "", dir.write("staidx0.mul", staidx),
                              dir.write("statics0.mul", statics), 16, 16));
    }
};

struct Rig
{
    TestMap assets;
    world::World world;
    world::PacketHandlers handlers;
    WorldTileSource tiles{world, assets.tiledata};
    std::vector<std::vector<uint8_t>> sent;
    uint64_t clock = 10000;
    MovementSystem movement{world, tiles, [this](std::span<const uint8_t> b) { sent.emplace_back(b.begin(), b.end()); }};

    Rig()
    {
        tiles.setMap(&assets.map);
        movement.now = [this] { return clock; };
        movement.install(handlers);

        world::Player& p = world.createPlayer(0x00000001);
        p.x = 5;
        p.y = 6;
        p.z = 0;
        p.direction = world::Direction::North;
        p.stamina = 50;
        p.staminaMax = 50;
        world.mapIndex = 0;
    }

    void server(std::vector<uint8_t> packet, size_t header = 1) { handlers.handle(world, packet, header); }
};

}  // namespace

TEST_CASE("movement system: tile source reads land, statics and world objects")
{
    Rig rig;
    std::vector<TileObject> out;

    REQUIRE(rig.tiles.gatherTile(5, 4, out));
    REQUIRE(out.size() == 2);
    CHECK(out[0].kind == TileObjectKind::Land);
    CHECK(out[1].kind == TileObjectKind::Static);
    CHECK(out[1].height == 20);
    CHECK(out[1].has(tile_flag::Impassable));

    out.clear();
    CHECK_FALSE(rig.tiles.gatherTile(16, 0, out));

    world::Mobile& orc = rig.world.getOrCreateMobile(0x00000055);
    orc.x = 7;
    orc.y = 7;
    rig.tiles.rebuild();
    out.clear();
    REQUIRE(rig.tiles.gatherTile(7, 7, out));
    REQUIRE(out.size() == 2);
    CHECK(out[1].kind == TileObjectKind::Mobile);

    // The player is not an obstacle to itself.
    out.clear();
    REQUIRE(rig.tiles.gatherTile(5, 6, out));
    CHECK(out.size() == 1);
}

TEST_CASE("movement system: multi components stand on their own tiles")
{
    Rig rig;
    const std::vector<assets::MultiComponent> parts{
        {0x0001, 0, 0, 0, 0, false},  // invisible
        {0x0001, 1, -1, 0, 1, true},  // the wall graphic, one tile east and north
    };
    rig.tiles.multiComponents = [&](uint16_t id) { return id == 0x0010 ? &parts : nullptr; };

    world::Item& house = rig.world.getOrCreateItem(0x40000020);
    house.isMulti = true;
    house.graphic = 0x0010;
    house.x = 9;
    house.y = 9;
    house.z = 3;
    rig.tiles.rebuild();

    std::vector<TileObject> out;
    REQUIRE(rig.tiles.gatherTile(9, 9, out));
    CHECK(out.size() == 1);

    out.clear();
    REQUIRE(rig.tiles.gatherTile(10, 8, out));
    REQUIRE(out.size() == 2);
    CHECK(out[1].kind == TileObjectKind::Multi);
    CHECK(out[1].z == 3);
    CHECK(out[1].height == 20);
    CHECK(out[1].has(tile_flag::Impassable));
}

TEST_CASE("movement system: steps, confirms and fast-walk keys through the packet handlers")
{
    Rig rig;
    rig.tiles.rebuild();

    rig.server({0xBF, 0x00, 0x09, 0x00, 0x02, 0xCA, 0xFE, 0xBA, 0xBE}, 3);

    WalkResult r = rig.movement.walk(Direction::North, false);
    REQUIRE(r.kind == WalkResult::Kind::Sent);
    REQUIRE(rig.sent.size() == 1);
    CHECK(rig.sent[0] == std::vector<uint8_t>{0x02, 0x00, 0x00, 0xCA, 0xFE, 0xBA, 0xBE});
    REQUIRE(rig.world.player()->steps.size() == 1);
    CHECK(rig.world.player()->steps.back().y == 5);

    // The wall at (5,4) stops the next step.
    rig.clock += 1000;
    CHECK(rig.movement.walk(Direction::North, false).kind == WalkResult::Kind::Rejected);

    rig.server({0x22, 0x00, 0x05});
    CHECK(rig.movement.walker().unacceptedCount() == 0);
    CHECK(rig.world.player()->notoriety == world::Notoriety::Enemy);
    CHECK(rig.world.rangeY == 5);

    // The step animates over 400 ms, then the player stands on (5, 5).
    rig.movement.update(std::nullopt, 0);
    rig.clock += 400;
    rig.movement.update(std::nullopt, 0);
    CHECK(rig.world.player()->y == 5);
    CHECK(rig.world.player()->steps.empty());
    CHECK(rig.movement.walker().stepsCount() == 0);

    // An unknown sequence asks the server to resync.
    rig.server({0x22, 0x42, 0x01});
    REQUIRE(rig.sent.size() == 2);
    CHECK(rig.sent[1] == std::vector<uint8_t>{0x22, 0x00, 0x00});
}

TEST_CASE("movement system: deny snaps the player back")
{
    Rig rig;
    rig.tiles.rebuild();

    REQUIRE(rig.movement.walk(Direction::North, false).kind == WalkResult::Kind::Sent);
    rig.server({0x21, 0x00, 0x00, 0x05, 0x00, 0x06, 0x02, 0x00});

    const world::Player* p = rig.world.player();
    CHECK((p->x == 5 && p->y == 6));
    CHECK(p->direction == world::Direction::East);
    CHECK(p->steps.empty());
    CHECK(rig.movement.walker().stepsCount() == 0);

    // Facing east now, so asking east steps rather than turns.
    rig.clock += 1000;
    WalkResult r = rig.movement.walk(Direction::East, false);
    CHECK_FALSE(r.turnOnly);
    CHECK(r.x == 6);
}

TEST_CASE("movement system: server moves and server pathfinding")
{
    Rig rig;
    rig.tiles.rebuild();

    rig.server({0x97, 0x84});  // run south
    REQUIRE(rig.sent.size() == 1);
    CHECK(rig.sent[0][1] == 0x84);

    rig.server({0x38, 0x00, 0x05, 0x00, 0x02, 0x00, 0x00});
    CHECK(rig.movement.autoWalker().active());
    const auto& path = rig.movement.pathfinder().path();
    REQUIRE_FALSE(path.empty());
    CHECK((path.back().x == 5 && path.back().y == 2));
}

TEST_CASE("movement system: auto-walk and input drive walks each frame")
{
    Rig rig;
    rig.tiles.rebuild();

    REQUIRE(rig.movement.walkTo(8, 6, 0, 0));
    for (int frame = 0; frame < 60 && rig.movement.autoWalker().active(); frame++)
    {
        rig.clock += 50;
        rig.movement.update(std::nullopt, 0);
        // The server accepts everything at once.
        for (int s = 0; s < rig.movement.walker().stepsCount(); s++)
        {
            const StepInfo& info = rig.movement.walker().step(s);
            if (!info.accepted)
            {
                rig.server({0x22, info.sequence, 0x01});
            }
        }
    }
    for (int frame = 0; frame < 40; frame++)
    {
        rig.clock += 50;
        rig.movement.update(std::nullopt, 0);
    }
    CHECK(rig.world.player()->x == 8);
    CHECK(rig.world.player()->y == 6);

    // Mouse input cancels auto-walk.
    REQUIRE(rig.movement.walkTo(2, 6, 0, 0));
    MovementIntent intent;
    intent.direction = Direction::South;
    intent.fromMouse = true;
    intent.cancelAutoWalk = true;
    rig.clock += 1000;
    rig.movement.update(intent, 0);
    CHECK_FALSE(rig.movement.autoWalker().active());
}
