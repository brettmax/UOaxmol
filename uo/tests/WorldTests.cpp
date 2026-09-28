// SPDX-License-Identifier: BSD-2-Clause
// Behaviour carried over from the first uo::game::World slice, now covered by uo::world.
// Walk prediction moved to uo::movement::Walker (see MovementTests.cpp).
#include "TestUtil.h"
#include "doctest.h"

#include "uo/net/PacketTable.h"
#include "uo/world/PacketHandlers.h"

#include <string>
#include <vector>

using namespace uo;
using namespace uo::world;
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

void setLength(Bytes& p)
{
    p[1] = static_cast<std::uint8_t>(p.size() >> 8);
    p[2] = static_cast<std::uint8_t>(p.size());
}

struct Mirror : WorldListener
{
    int created = 0;
    int updated = 0;
    std::vector<Serial> removed;
    int messages = 0;

    void onEntityCreated(Entity&) override { ++created; }
    void onEntityUpdated(Entity&) override { ++updated; }
    void onEntityRemoved(Serial s, EntityKind) override { removed.push_back(s); }
    void onMessage(const Message&) override { ++messages; }
};

// A logged-in world, fed through the packet table like the client does.
struct Game
{
    World world;
    PacketHandlers handlers;
    net::PacketTable table;
    Mirror mirror;

    explicit Game(ClientVersion version = makeVersion(7, 0, 15, 1)) : world(version), table(version)
    {
        world.setListener(&mirror);
        REQUIRE(handle(enterWorld(1, 10, 10, 2)));
    }

    bool handle(const Bytes& p) { return handlers.handle(world, p, table); }
};
}  // namespace

TEST_CASE("mobiles arrive with their equipment")
{
    Game g(makeVersion(7, 0, 40, 0));  // 7.0.33.1+ always sends equipment hues
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
    setLength(p);

    REQUIRE(g.handle(p));
    const Mobile* m = g.world.mobile(0x42);
    REQUIRE(m);
    CHECK(m->isMobile());
    CHECK(m->x == 100);
    CHECK(m->notoriety == Notoriety(1));
    REQUIRE(m->contents().size() == 1);
    const Item* cloak = g.world.item(m->contents()[0]);
    REQUIRE(cloak);
    CHECK(cloak->graphic == 0x1515);
    CHECK(cloak->layer == Layer(5));
    CHECK(cloak->hue == 0x21);
}

TEST_CASE("classic world items decode their optional fields")
{
    Game g;
    const int created = g.mirror.created;

    Bytes p{0x1A, 0, 0};
    be32(p, 0x80000000u | 0x40000010u);  // amount follows
    be16(p, 0x0EED);
    be16(p, 500);                     // amount
    be16(p, 1000);                    // x
    be16(p, 0x8000 | 0x4000 | 1200);  // y, hue and flags follow
    p.push_back(static_cast<std::uint8_t>(-3));
    be16(p, 0x0035);
    p.push_back(0x20);
    setLength(p);

    REQUIRE(g.handle(p));
    const Item* e = g.world.item(0x40000010);
    REQUIRE(e);
    CHECK_FALSE(e->isMobile());
    CHECK(e->amount == 500);
    CHECK(e->x == 1000);
    CHECK(e->y == 1200);
    CHECK(e->z == -3);
    CHECK(e->hue == 0x35);
    CHECK(e->flagBits == 0x20);
    CHECK(g.mirror.created == created + 1);

    Bytes del{0x1D};
    be32(del, 0x40000010);
    REQUIRE(g.handle(del));
    CHECK(g.world.item(0x40000010) == nullptr);
    CHECK(g.mirror.removed == std::vector<Serial>{0x40000010});
}

TEST_CASE("speech lands in the journal")
{
    Game g;
    Bytes p{0xAE, 0, 0};
    be32(p, 0x42);
    be16(p, 0x0190);
    p.push_back(0);
    be16(p, 0x3B2);
    be16(p, 3);
    ascii(p, "ENU", 4);
    ascii(p, "Brett", 30);
    be16(p, 'H'), be16(p, 0x00E9), be16(p, 0);
    setLength(p);

    REQUIRE(g.handle(p));
    REQUIRE(g.world.journal().size() == 1);
    const Message& m = g.world.journal()[0];
    CHECK(m.name == "Brett");
    CHECK(m.text == "H\xC3\xA9");
    CHECK(m.font == 3);
    CHECK(m.unicode);
    CHECK(g.mirror.messages == 1);
}

TEST_CASE("ASCII speech decodes Windows-1252")
{
    Game g;
    Bytes p{0x1C, 0, 0};
    be32(p, 0xFFFFFFFF);
    be16(p, 0xFFFF);
    p.push_back(0);
    be16(p, 0x3B2), be16(p, 3);
    ascii(p, "System", 30);
    for (std::uint8_t b : {std::uint8_t('5'), std::uint8_t(0x80), std::uint8_t(0xE9), std::uint8_t(0)})
        p.push_back(b);
    setLength(p);

    REQUIRE(g.handle(p));
    const Message& m = g.world.journal().back();
    CHECK(m.text == "5\xE2\x82\xAC\xC3\xA9");  // "5€é"
    CHECK_FALSE(m.unicode);
}

TEST_CASE("cliloc messages keep their number and arguments for the text layer")
{
    Game g;
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
    setLength(p);

    REQUIRE(g.handle(p));
    const Message& j = g.world.journal().back();
    CHECK(j.cliloc == 500000);
    CHECK(j.clilocArgs == "Brett\t#1000");
    CHECK(j.unicode);
    CHECK(j.text.empty());
}

TEST_CASE("affixed cliloc messages keep the affix apart")
{
    Game g;
    Bytes p{0xCC, 0, 0};
    be32(p, 0xFFFFFFFF);
    be16(p, 0xFFFF);
    p.push_back(0);
    be16(p, 0x3B2), be16(p, 3);
    be32(p, 1042971);
    p.push_back(0x01);  // prepend
    ascii(p, "System", 30);
    ascii(p, "Hi ", 4);
    for (char c : std::string("x"))
        be16(p, static_cast<std::uint16_t>(c));
    be16(p, 0);
    setLength(p);

    REQUIRE(g.handle(p));
    const Message& j = g.world.journal().back();
    CHECK(j.cliloc == 1042971);
    CHECK(j.affix == "Hi ");
    CHECK(j.affixPrepend);
    CHECK(j.clilocArgs == "x");
    CHECK(j.text.empty());
}

TEST_CASE("a resolver translates cliloc messages in place")
{
    struct Table : ClilocResolver
    {
        std::string get(uint32_t) override { return "You see: ~1_NAME~"; }
        std::optional<std::string> translate(uint32_t, std::string_view args, bool) override
        {
            return "You see: " + std::string(args);
        }
    } table;

    Game g;
    g.world.setClilocs(&table);
    Bytes p{0xC1, 0, 0};
    be32(p, 0xFFFFFFFF);
    be16(p, 0xFFFF);
    p.push_back(0);
    be16(p, 0x3B2), be16(p, 3);
    be32(p, 1005000);
    ascii(p, "System", 30);
    for (char c : std::string("an orc"))
        le16(p, static_cast<std::uint16_t>(c));
    le16(p, 0);
    setLength(p);

    REQUIRE(g.handle(p));
    CHECK(g.world.journal().back().text == "You see: an orc");
}

TEST_CASE("status and stat packets update the entity")
{
    Game g;
    Bytes hits{0xA1};
    be32(hits, 1), be16(hits, 100), be16(hits, 42);
    REQUIRE(g.handle(hits));
    CHECK(g.world.player()->hits == 42);
    CHECK(g.world.player()->hitsMax == 100);
}

TEST_CASE("0xBF 0x08 switches the map")
{
    Game g;
    struct Maps : Mirror
    {
        int map = -1;
        void onMapChanged(int m) override { map = m; }
    } maps;
    g.world.setListener(&maps);

    Bytes p{0xBF, 0, 0};
    be16(p, 0x08);
    p.push_back(2);
    setLength(p);
    REQUIRE(g.handle(p));
    CHECK(g.world.mapIndex == 2);
    CHECK(maps.map == 2);
}

TEST_CASE("BinaryReader reads 64-bit big-endian values")
{
    Bytes b;
    be32(b, 0x01020304), be32(b, 0x05060708);
    io::BinaryReader r(b);
    CHECK(r.readU64BE() == 0x0102030405060708ull);
    CHECK(r.readU64BE() == 0);
    CHECK(r.overflowed());
}
