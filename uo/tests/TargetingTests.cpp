// SPDX-License-Identifier: BSD-2-Clause
#include "uo/world/PacketHandlers.h"
#include "uo/world/Targeting.h"
#include "uo/world/World.h"

#include "doctest.h"

#include <vector>

using namespace uo::world;

namespace
{

struct Rig
{
    World world;
    PacketHandlers handlers;
    std::vector<std::vector<uint8_t>> sent;
    int multiCleared = 0;
    bool answerQuery = true;
    std::function<void()> pendingQuery;
    Targeting targeting{world, hooks()};

    Rig()
    {
        targeting.install(handlers);
        Player& p = world.createPlayer(0x00000001);
        p.notoriety = Notoriety::Innocent;
        world.mapIndex = 0;
    }

    TargetingHooks hooks()
    {
        TargetingHooks h;
        h.send = [this](std::span<const uint8_t> b) { sent.emplace_back(b.begin(), b.end()); };
        h.clearMultiPreview = [this] { multiCleared++; };
        h.askCriminalAction = [this](std::function<void()> proceed) {
            pendingQuery = std::move(proceed);
            return answerQuery;
        };
        return h;
    }

    void serverTarget(uint8_t state, uint32_t cursor, uint8_t type)
    {
        std::vector<uint8_t> p{0x6C, state, static_cast<uint8_t>(cursor >> 24), static_cast<uint8_t>(cursor >> 16),
                               static_cast<uint8_t>(cursor >> 8), static_cast<uint8_t>(cursor), type};
        p.resize(19, 0);
        handlers.handle(world, p, 1);
    }

    Mobile& mobile(Serial s, uint16_t x, uint16_t y, int8_t z, Notoriety n)
    {
        Mobile& m = world.getOrCreateMobile(s);
        m.graphic = 0x0011;
        m.x = x;
        m.y = y;
        m.z = z;
        m.notoriety = n;
        return m;
    }
};

}  // namespace

TEST_CASE("targeting: server cursor, object target and packet layout")
{
    Rig rig;
    rig.serverTarget(0, 0x01020304, 1);
    CHECK(rig.world.target.isTargeting);
    CHECK(rig.world.target.state == CursorTarget::Object);
    CHECK(rig.world.target.type == TargetType::Harmful);

    rig.mobile(0x00001234, 1500, 1600, -3, Notoriety::Enemy);
    rig.targeting.target(0x00001234);

    REQUIRE(rig.sent.size() == 1);
    const std::vector<uint8_t> expect{0x6C, 0x00, 0x01, 0x02, 0x03, 0x04, 0x01, 0x00, 0x00, 0x12,
                                      0x34, 0x05, 0xDC, 0x06, 0x40, 0xFF, 0xFD, 0x00, 0x11};
    CHECK(rig.sent[0] == expect);
    CHECK_FALSE(rig.targeting.isTargeting());
    CHECK(rig.targeting.lastTarget().serial == 0x1234);
    CHECK(rig.targeting.lastTarget().isEntity());
}

TEST_CASE("targeting: land and statics send position targets")
{
    Rig rig;

    // Object cursors ignore land.
    rig.serverTarget(0, 7, 0);
    rig.targeting.target(0, 100, 200, 5);
    CHECK(rig.sent.empty());
    CHECK(rig.targeting.isTargeting());

    rig.serverTarget(1, 7, 0);
    rig.targeting.target(0x0B10, 100, 200, 5, 6);  // a 6-high surface: target its top
    REQUIRE(rig.sent.size() == 1);
    CHECK(rig.sent[0][1] == 0x01);
    CHECK(rig.sent[0][16] == 11);
    CHECK((rig.sent[0][17] == 0x0B && rig.sent[0][18] == 0x10));
    CHECK(rig.targeting.lastTarget().isStatic());

    // Target-last replays the body under the new cursor.
    rig.serverTarget(1, 9, 2);
    rig.targeting.targetLast();
    REQUIRE(rig.sent.size() == 2);
    CHECK(rig.sent[1][5] == 9);
    CHECK(rig.sent[1][6] == 2);
    CHECK(rig.sent[1][16] == 11);
}

TEST_CASE("targeting: cancel, and server cancel answers with the old cursor")
{
    Rig rig;

    rig.serverTarget(0, 5, 0);
    rig.targeting.cancel();
    REQUIRE(rig.sent.size() == 1);
    CHECK(rig.sent[0] ==
          std::vector<uint8_t>{0x6C, 0x00, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0xFF, 0xFF, 0xFF, 0xFF, 0, 0, 0, 0});
    CHECK_FALSE(rig.targeting.isTargeting());

    rig.sent.clear();
    rig.serverTarget(0, 5, 0);
    rig.serverTarget(0, 9, 3);
    REQUIRE(rig.sent.size() == 1);
    CHECK(rig.sent[0][5] == 5);
    CHECK(rig.sent[0][6] == 3);
    CHECK_FALSE(rig.targeting.isTargeting());
    CHECK(rig.world.target.cursorId == 9);
}

TEST_CASE("targeting: harmful action on an innocent asks first")
{
    Rig rig;
    rig.mobile(0x00000077, 10, 10, 0, Notoriety::Innocent);

    rig.serverTarget(0, 1, 1);
    rig.targeting.target(0x00000077);
    CHECK(rig.sent.empty());
    REQUIRE(rig.pendingQuery);

    rig.pendingQuery();
    CHECK(rig.sent.size() == 1);
    CHECK_FALSE(rig.targeting.isTargeting());

    // A query already open: target straight away.
    rig.sent.clear();
    rig.answerQuery = false;
    rig.serverTarget(0, 1, 1);
    rig.targeting.target(0x00000077);
    CHECK(rig.sent.size() == 1);
}

TEST_CASE("targeting: multi placement and client-side cursors")
{
    Rig rig;

    std::vector<uint8_t> p(30, 0);
    p[0] = 0x99;
    p[5] = 0x2A;
    p[19] = 0x64;  // model
    rig.handlers.handle(rig.world, p, 1);
    REQUIRE(rig.world.target.multi.has_value());
    CHECK(rig.world.target.state == CursorTarget::MultiPlacement);

    rig.targeting.sendMultiTarget(1000, 1000, 0);
    CHECK(rig.sent.size() == 1);
    CHECK_FALSE(rig.world.target.multi.has_value());
    CHECK(rig.multiCleared == 1);

    // A callback cursor picks locally and sends nothing.
    rig.sent.clear();
    Item& item = rig.world.getOrCreateItem(0x40000001);
    item.amount = 3;
    std::optional<Serial> picked;
    rig.targeting.setCallback([&](const Targeting::Picked* pk) {
        if (pk != nullptr)
        {
            picked = pk->serial;
        }
    });
    CHECK(rig.targeting.isTargeting());
    rig.targeting.target(0x40000001);
    CHECK(picked == 0x40000001u);
    CHECK(rig.sent.empty());
    CHECK_FALSE(rig.targeting.isTargeting());

    bool cancelled = false;
    rig.targeting.setCallback([&](const Targeting::Picked* pk) { cancelled = pk == nullptr; });
    rig.targeting.cancel();
    CHECK(cancelled);
    CHECK(rig.sent.empty());

    // A server cursor replaces a client-side one.
    cancelled = false;
    rig.targeting.setCallback([&](const Targeting::Picked* pk) { cancelled = pk == nullptr; });
    rig.serverTarget(0, 4, 0);
    CHECK(cancelled);
    CHECK(rig.targeting.clientCursor() == Targeting::ClientCursor::None);
    CHECK(rig.world.target.cursorId == 4);
}
