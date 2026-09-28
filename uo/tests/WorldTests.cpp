// SPDX-License-Identifier: BSD-2-Clause
#include "TestUtil.h"
#include "doctest.h"

#include "uo/game/World.h"

using namespace uo;
using namespace uo::game;
using namespace uotest;

namespace
{
Bytes enterWorld(std::uint32_t serial, std::uint16_t x, std::uint16_t y, std::uint8_t dir)
{
    Bytes p{0x1B};
    be32(p, serial);
    be32(p, 0);
    be16(p, 0x0190);
    be16(p, x), be16(p, y), be16(p, 0);
    p.push_back(dir);
    p.resize(37, 0);
    return p;
}
}  // namespace

TEST_CASE("mobiles arrive with their equipment")
{
    World w(makeVersion(7, 0, 40, 0));  // 7.0.33.1+ always sends equipment hues
    Bytes p{0x78, 0, 0};
    be32(p, 0x42);
    be16(p, 0x0191);
    be16(p, 100), be16(p, 200);
    p.push_back(5);   // z
    p.push_back(4);   // direction
    be16(p, 0x83EA);  // hue
    p.push_back(0);
    p.push_back(1);   // innocent
    be32(p, 0x40000001), be16(p, 0x1515), p.push_back(0x05), be16(p, 0x0021);
    be32(p, 0);
    p[2] = static_cast<std::uint8_t>(p.size());

    REQUIRE(w.handle(p));
    const Entity* m = w.find(0x42);
    REQUIRE(m);
    CHECK(m->isMobile());
    CHECK(m->x == 100);
    CHECK(m->notoriety == 1);
    REQUIRE(m->equipment.size() == 1);
    CHECK(m->equipment[0].graphic == 0x1515);
    CHECK(m->equipment[0].layer == 5);
    CHECK(m->equipment[0].hue == 0x21);
}

TEST_CASE("classic world items decode their optional fields")
{
    World w;
    int updates = 0;
    w.onEntityUpdated = [&](const Entity&) { ++updates; };

    Bytes p{0x1A, 0, 0};
    be32(p, 0x80000000u | 0x40000010u);  // amount follows
    be16(p, 0x0EED);
    be16(p, 500);             // amount
    be16(p, 1000);            // x
    be16(p, 0x8000 | 0x4000 | 1200);  // y, hue and flags follow
    p.push_back(static_cast<std::uint8_t>(-3));
    be16(p, 0x0035);
    p.push_back(0x20);
    p[2] = static_cast<std::uint8_t>(p.size());

    REQUIRE(w.handle(p));
    const Entity* e = w.find(0x40000010);
    REQUIRE(e);
    CHECK_FALSE(e->isMobile());
    CHECK(e->amount == 500);
    CHECK(e->x == 1000);
    CHECK(e->y == 1200);
    CHECK(e->z == -3);
    CHECK(e->hue == 0x35);
    CHECK(e->flags == 0x20);
    CHECK(updates == 1);

    Bytes del{0x1D};
    be32(del, 0x40000010);
    w.handle(del);
    CHECK(w.find(0x40000010) == nullptr);
}

TEST_CASE("walking predicts steps and reconciles with the server")
{
    World w;
    w.handle(enterWorld(1, 10, 10, static_cast<std::uint8_t>(Direction::East)));

    auto turn = w.requestWalk(Direction::South, false);  // facing east: this only turns
    REQUIRE(turn);
    CHECK((*turn)[1] == 4);
    CHECK((*turn)[2] == 0);  // first sequence after login is 0
    CHECK(w.player()->x == 10);
    CHECK(w.player()->y == 10);

    auto step = w.requestWalk(Direction::South, true);
    REQUIRE(step);
    CHECK((*step)[1] == (4 | 0x80));
    CHECK((*step)[2] == 1);
    CHECK(w.player()->y == 11);
    CHECK(w.pendingSteps() == 2);

    w.handle(Bytes{0x22, 0, 1});
    CHECK(w.pendingSteps() == 1);

    Bytes deny{0x21, 1};
    be16(deny, 10), be16(deny, 10);
    deny.push_back(4), deny.push_back(0);
    w.handle(deny);
    CHECK(w.pendingSteps() == 0);
    CHECK(w.player()->y == 10);

    for (std::size_t i = 0; i < World::kMaxPendingSteps; ++i)
        REQUIRE(w.requestWalk(Direction::South, false));
    CHECK_FALSE(w.requestWalk(Direction::South, false));
}

TEST_CASE("speech lands in the journal")
{
    World w;
    Bytes p{0xAE, 0, 0};
    be32(p, 0x42);
    be16(p, 0x0190);
    p.push_back(0);
    be16(p, 0x3B2);
    be16(p, 3);
    ascii(p, "ENU", 4);
    ascii(p, "Brett", 30);
    be16(p, 'H'), be16(p, 0x00E9), be16(p, 0);
    p[2] = static_cast<std::uint8_t>(p.size());
    REQUIRE(w.handle(p));
    REQUIRE(w.journal().size() == 1);
    CHECK(w.journal()[0].name == "Brett");
    CHECK(w.journal()[0].text == "H\xC3\xA9");
}

TEST_CASE("cliloc messages keep their number and arguments for the text layer")
{
    World w;
    Bytes p{0xC1, 0, 0};
    be32(p, 0xFFFFFFFF);
    be16(p, 0xFFFF);
    p.push_back(6);
    be16(p, 0x3B2);
    be16(p, 3);
    be32(p, 500000);
    ascii(p, "System", 30);
    for (char c : std::string("Brett\t#1000"))
        le16(p, static_cast<std::uint16_t>(c));
    le16(p, 0);
    p[2] = static_cast<std::uint8_t>(p.size());
    REQUIRE(w.handle(p));
    const JournalEntry& j = w.journal().back();
    CHECK(j.clilocNumber == 500000);
    CHECK(j.clilocArgs == "Brett\t#1000");
    CHECK(j.unicode);
    CHECK(j.text.empty());
}
