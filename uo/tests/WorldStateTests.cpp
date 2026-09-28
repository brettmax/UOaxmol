// SPDX-License-Identifier: BSD-2-Clause
// uo::world packet handlers, exercised with the T2A-era packets ModernUO sends.
#include "TestUtil.h"
#include "doctest.h"

#include "uo/net/PacketTable.h"
#include "uo/world/PacketHandlers.h"

#include <algorithm>
#include <string>
#include <vector>

using namespace uo;
using namespace uo::world;
using namespace uotest;

namespace
{

constexpr Serial kPlayer = 0x00000010;
constexpr Serial kOrc = 0x00000042;
constexpr Serial kBackpack = 0x40000100;

struct Recorder : WorldListener
{
    std::vector<Serial> created;
    std::vector<Serial> removed;
    std::vector<Serial> openedContainers;
    std::vector<Serial> equipmentChanged;
    std::vector<std::pair<Serial, Serial>> deaths; // owner, corpse
    std::vector<TradeEvent> trades;
    std::vector<int> mapChanges;
    int targetChanges = 0;
    int partyChanges = 0;
    int skillsChanged = 0;
    int statsChanged = 0;

    void onEntityCreated(Entity& e) override { created.push_back(e.serial); }
    void onEntityRemoved(Serial s, EntityKind) override { removed.push_back(s); }
    void onOpenContainer(Item& i, uint16_t) override { openedContainers.push_back(i.serial); }
    void onEquipmentChanged(Serial m) override { equipmentChanged.push_back(m); }
    void onMobileDied(Mobile& m, Serial corpse, bool) override { deaths.emplace_back(m.serial, corpse); }
    void onTrade(const TradeEvent& t) override { trades.push_back(t); }
    void onMapChanged(int index) override { mapChanges.push_back(index); }
    void onTargetCursorChanged() override { ++targetChanges; }
    void onPartyChanged() override { ++partyChanges; }
    void onSkillsChanged(int, bool) override { ++skillsChanged; }
    void onStatsChanged(Entity&) override { ++statsChanged; }
};

struct Requests : ServerRequests
{
    std::vector<Serial> statusRequests;
    std::vector<Serial> skillRequests;
    int versionRequests = 0;

    void clientVersion() override { ++versionRequests; }
    void requestSkills(Serial s) override { skillRequests.push_back(s); }
    void requestMobileStatus(Serial s) override { statusRequests.push_back(s); }
};

// Variable-length packet: id, then a 2-byte length patched by finish().
Bytes var(uint8_t id)
{
    return Bytes{id, 0, 0};
}
Bytes finish(Bytes p)
{
    p[1] = uint8_t(p.size() >> 8);
    p[2] = uint8_t(p.size() & 0xFF);
    return p;
}
void unicodeBE(Bytes& b, const std::string& s)
{
    for (char c : s)
        be16(b, uint8_t(c));
    be16(b, 0);
}

Bytes enterWorld(Serial serial, uint16_t x, uint16_t y)
{
    Bytes p{0x1B};
    be32(p, serial);
    be32(p, 0);
    be16(p, 0x0190);
    be16(p, x), be16(p, y), be16(p, 0);
    p.push_back(2); // east
    p.resize(37, 0);
    return p;
}

struct Fixture
{
    World world;
    PacketHandlers handlers;
    Recorder events;
    Requests requests;

    Fixture()
    {
        world.setListener(&events);
        world.setRequests(&requests);
    }

    bool fixed(const Bytes& p) { return handlers.handle(world, p, 1); }
    bool variable(const Bytes& p) { return handlers.handle(world, p, 3); }

    void login()
    {
        REQUIRE(fixed(enterWorld(kPlayer, 1000, 2000)));
        // Backpack, so container packets have somewhere to go.
        Bytes equip{0x2E};
        be32(equip, kBackpack);
        be16(equip, 0x0E75);
        equip.push_back(0);
        equip.push_back(uint8_t(Layer::Backpack));
        be32(equip, kPlayer);
        be16(equip, 0);
        REQUIRE(fixed(equip));
    }

    // 0x78 for an orc wearing one piece of hued equipment (pre-7.0.33 layout).
    void spawnOrc(uint16_t x = 1001, uint16_t y = 2000)
    {
        Bytes p = var(0x78);
        be32(p, kOrc);
        be16(p, 0x0011);
        be16(p, x), be16(p, y);
        p.push_back(0);   // z
        p.push_back(4);   // south
        be16(p, 0x0000);  // hue
        p.push_back(0);   // flags
        p.push_back(6);   // notoriety: enemy
        be32(p, 0x40000200), be16(p, 0x8000 | 0x1415), p.push_back(uint8_t(Layer::Torso)), be16(p, 0x0021);
        be32(p, 0);
        REQUIRE(variable(finish(p)));
    }

    Bytes containedItem(Serial serial, uint16_t graphic, uint16_t amount, uint16_t x, uint16_t y, Serial container)
    {
        Bytes p;
        be32(p, serial);
        be16(p, graphic);
        p.push_back(0);
        be16(p, amount);
        be16(p, x), be16(p, y);
        p.push_back(0); // grid index (6.0.1.8+)
        be32(p, container);
        be16(p, 0);
        return p;
    }
};

} // namespace

TEST_CASE("0x1B enters the world and asks for the player's details")
{
    Fixture f;
    REQUIRE(f.fixed(enterWorld(kPlayer, 1000, 2000)));

    REQUIRE(f.world.player() != nullptr);
    CHECK(f.world.player()->serial == kPlayer);
    CHECK(f.world.player()->x == 1000);
    CHECK(f.world.player()->y == 2000);
    CHECK(f.world.player()->direction == Direction::East);
    CHECK(f.world.inGame());
    CHECK(f.world.mapIndex == 0);
    CHECK(f.requests.versionRequests == 1);
    CHECK(f.requests.skillRequests == std::vector<Serial>{kPlayer});
}

TEST_CASE("packets before the player exists are ignored")
{
    Fixture f;
    Bytes p = var(0x1A);
    be32(p, 0x40000001);
    be16(p, 0x0EED);
    be16(p, 10), be16(p, 10);
    p.push_back(0);
    CHECK(f.variable(finish(p)));
    CHECK(f.world.itemCount() == 0);
}

TEST_CASE("0x78 creates a mobile with its equipment")
{
    Fixture f;
    f.login();
    f.spawnOrc();

    Mobile* orc = f.world.mobile(kOrc);
    REQUIRE(orc != nullptr);
    CHECK(orc->graphic == 0x0011);
    CHECK(orc->notoriety == Notoriety(6));
    CHECK(orc->x == 1001);
    REQUIRE(orc->contents().size() == 1);

    Item* shirt = f.world.item(0x40000200);
    REQUIRE(shirt != nullptr);
    CHECK(shirt->graphic == 0x1415);
    CHECK(shirt->hue == 0x0021);
    CHECK(shirt->layer == Layer::Torso);
    CHECK(shirt->container == kOrc);
    CHECK(f.world.findItemByLayer(*orc, Layer::Torso) == shirt);

    // New mobiles get a status request so health bars fill in.
    CHECK(std::find(f.requests.statusRequests.begin(), f.requests.statusRequests.end(), kOrc) !=
          f.requests.statusRequests.end());

    SUBCASE("a resend replaces the equipment")
    {
        Bytes p = var(0x78);
        be32(p, kOrc);
        be16(p, 0x0011);
        be16(p, 1001), be16(p, 2000);
        p.push_back(0), p.push_back(4);
        be16(p, 0), p.push_back(0), p.push_back(6);
        be32(p, 0);
        REQUIRE(f.variable(finish(p)));
        CHECK(f.world.item(0x40000200) == nullptr);
        CHECK(f.world.mobile(kOrc)->contents().empty());
    }
}

TEST_CASE("0x78 reads hues unconditionally from 7.0.33.1")
{
    Fixture f;
    f.world.clientVersion = makeVersion(7, 0, 40, 0);
    f.login();

    Bytes p = var(0x78);
    be32(p, kOrc);
    be16(p, 0x0190);
    be16(p, 1001), be16(p, 2000);
    p.push_back(0), p.push_back(4);
    be16(p, 0x83EA), p.push_back(0), p.push_back(1);
    be32(p, 0x40000001), be16(p, 0x1515), p.push_back(0x05), be16(p, 0x0021);
    be32(p, 0);
    REQUIRE(f.variable(finish(p)));

    Item* cloak = f.world.item(0x40000001);
    REQUIRE(cloak != nullptr);
    CHECK(cloak->graphic == 0x1515);
    CHECK(cloak->hue == 0x0021);
}

TEST_CASE("0x1A places a ground item with amount and hue")
{
    Fixture f;
    f.login();

    Bytes p = var(0x1A);
    be32(p, 0x80000000u | 0x40000300); // amount follows
    be16(p, 0x0EED);                   // gold
    be16(p, 250);
    be16(p, 1002);
    be16(p, 0x8000 | 2001); // hue follows
    p.push_back(5);         // z
    be16(p, 0x0035);
    REQUIRE(f.variable(finish(p)));

    Item* gold = f.world.item(0x40000300);
    REQUIRE(gold != nullptr);
    CHECK(gold->graphic == 0x0EED);
    CHECK(gold->amount == 250);
    CHECK(gold->x == 1002);
    CHECK(gold->y == 2001);
    CHECK(gold->z == 5);
    CHECK(gold->hue == 0x0035);
    CHECK(gold->onGround());
}

TEST_CASE("0x24 opens a container and 0x3C / 0x25 fill it")
{
    Fixture f;
    f.login();

    Bytes open{0x24};
    be32(open, kBackpack);
    be16(open, 0x003C);
    REQUIRE(f.fixed(open));
    CHECK(f.events.openedContainers == std::vector<Serial>{kBackpack});
    CHECK(f.world.item(kBackpack)->opened);

    Bytes list = var(0x3C);
    be16(list, 2);
    for (auto b : f.containedItem(0x40000400, 0x0F0E, 1, 40, 60, kBackpack))
        list.push_back(b);
    for (auto b : f.containedItem(0x40000401, 0x0EED, 500, 50, 70, kBackpack))
        list.push_back(b);
    REQUIRE(f.variable(finish(list)));

    Item* pack = f.world.item(kBackpack);
    REQUIRE(pack->contents().size() == 2);
    CHECK(f.world.item(0x40000401)->amount == 500);
    CHECK(f.world.item(0x40000401)->x == 50);
    CHECK(f.world.rootContainer(*f.world.item(0x40000400)) == kPlayer);

    Bytes single{0x25};
    for (auto b : f.containedItem(0x40000402, 0x1F4C, 3, 80, 90, kBackpack))
        single.push_back(b);
    REQUIRE(f.fixed(single));
    CHECK(pack->contents().size() == 3);
    CHECK(f.world.item(0x40000402)->container == kBackpack);

    SUBCASE("0x3C resends the whole container")
    {
        Bytes again = var(0x3C);
        be16(again, 1);
        for (auto b : f.containedItem(0x40000401, 0x0EED, 499, 50, 70, kBackpack))
            again.push_back(b);
        REQUIRE(f.variable(finish(again)));
        CHECK(pack->contents() == std::vector<Serial>{0x40000401});
        CHECK(f.world.item(0x40000400) == nullptr);
    }

    SUBCASE("0x1D deletes an item inside it")
    {
        Bytes del{0x1D};
        be32(del, 0x40000400);
        REQUIRE(f.fixed(del));
        CHECK(f.world.item(0x40000400) == nullptr);
        CHECK(pack->contents().size() == 2);
    }
}

TEST_CASE("0x2E equips an item on the player")
{
    Fixture f;
    f.login();

    Bytes p{0x2E};
    be32(p, 0x40000500);
    be16(p, 0x13B9); // viking sword
    p.push_back(0);
    p.push_back(uint8_t(Layer::OneHanded));
    be32(p, kPlayer);
    be16(p, 0);
    REQUIRE(f.fixed(p));

    Item* sword = f.world.item(0x40000500);
    REQUIRE(sword != nullptr);
    CHECK(sword->container == kPlayer);
    CHECK(f.world.findItemByLayer(*f.world.player(), Layer::OneHanded) == sword);
    CHECK(std::find(f.events.equipmentChanged.begin(), f.events.equipmentChanged.end(), kPlayer) !=
          f.events.equipmentChanged.end());
}

TEST_CASE("0x1D removes mobiles with their equipment but never the player")
{
    Fixture f;
    f.login();
    f.spawnOrc();

    Bytes del{0x1D};
    be32(del, kOrc);
    REQUIRE(f.fixed(del));
    CHECK(f.world.mobile(kOrc) == nullptr);
    CHECK(f.world.item(0x40000200) == nullptr);

    Bytes self{0x1D};
    be32(self, kPlayer);
    REQUIRE(f.fixed(self));
    CHECK(f.world.player() != nullptr);
    CHECK(f.world.mobile(kPlayer) != nullptr);
}

TEST_CASE("0x11 fills the player's status")
{
    Fixture f;
    f.login();

    Bytes p = var(0x11);
    be32(p, kPlayer);
    ascii(p, "Brett", 30);
    be16(p, 80), be16(p, 100); // hits
    p.push_back(0);            // not renamable
    p.push_back(1);            // T2A status
    p.push_back(0);            // male
    be16(p, 90), be16(p, 50), be16(p, 40); // str dex int
    be16(p, 45), be16(p, 50);              // stamina
    be16(p, 30), be16(p, 40);              // mana
    be32(p, 1234);                         // gold
    be16(p, 7);                            // armor
    be16(p, 120);                          // weight
    REQUIRE(f.variable(finish(p)));

    Player& me = *f.world.player();
    CHECK(me.name == "Brett");
    CHECK(me.hits == 80);
    CHECK(me.hitsMax == 100);
    CHECK(me.strength == 90);
    CHECK(me.dexterity == 50);
    CHECK(me.intelligence == 40);
    CHECK(me.stamina == 45);
    CHECK(me.manaMax == 40);
    CHECK(me.gold == 1234);
    CHECK(me.physicalResistance == 7);
    CHECK(me.weight == 120);
    CHECK(me.weightMax == 7 * (90 / 2) + 40); // derived from strength for 5.0+ clients
}

TEST_CASE("0xA1-0xA3 update hits, mana and stamina")
{
    Fixture f;
    f.login();
    f.spawnOrc();

    Bytes hits{0xA1};
    be32(hits, kOrc), be16(hits, 60), be16(hits, 25);
    Bytes mana{0xA2};
    be32(mana, kOrc), be16(mana, 20), be16(mana, 10);
    Bytes stam{0xA3};
    be32(stam, kOrc), be16(stam, 70), be16(stam, 35);
    REQUIRE(f.fixed(hits));
    REQUIRE(f.fixed(mana));
    REQUIRE(f.fixed(stam));

    Mobile& orc = *f.world.mobile(kOrc);
    CHECK(orc.hitsMax == 60);
    CHECK(orc.hits == 25);
    CHECK(orc.manaMax == 20);
    CHECK(orc.mana == 10);
    CHECK(orc.staminaMax == 70);
    CHECK(orc.stamina == 35);
    CHECK(f.events.statsChanged == 3);
}

TEST_CASE("0x1C and 0xAE add speech to the journal")
{
    Fixture f;
    f.login();
    f.spawnOrc();

    Bytes talk = var(0x1C);
    be32(talk, kOrc);
    be16(talk, 0x0011);
    talk.push_back(uint8_t(MessageType::Regular));
    be16(talk, 0x0022), be16(talk, 3);
    ascii(talk, "an orc", 30);
    ascii(talk, "Grr!", 5);
    REQUIRE(f.variable(finish(talk)));

    Bytes uni = var(0xAE);
    be32(uni, 0xFFFFFFFF);
    be16(uni, 0xFFFF);
    uni.push_back(uint8_t(MessageType::System));
    be16(uni, 0x03B2), be16(uni, 3);
    ascii(uni, "ENU", 4);
    ascii(uni, "System", 30);
    unicodeBE(uni, "Welcome to Britannia");
    REQUIRE(f.variable(finish(uni)));

    const auto& journal = f.world.journal();
    REQUIRE(journal.size() == 2);
    CHECK(journal[0].name == "an orc");
    CHECK(journal[0].text == "Grr!");
    CHECK(journal[0].serial == kOrc);
    CHECK(journal[0].textType == TextType::Object);
    CHECK(journal[1].text == "Welcome to Britannia");
    CHECK(journal[1].unicode);
    CHECK(journal[1].textType == TextType::System);
}

TEST_CASE("0x6C starts and cancels a target cursor")
{
    Fixture f;
    f.login();

    Bytes start{0x6C, 0x00};
    be32(start, 0x1234);
    start.push_back(uint8_t(TargetType::Harmful));
    start.resize(19, 0);
    REQUIRE(f.fixed(start));
    CHECK(f.world.target.isTargeting);
    CHECK(f.world.target.cursorId == 0x1234);
    CHECK(f.world.target.type == TargetType::Harmful);

    Bytes cancel{0x6C, 0x00};
    be32(cancel, 0x1234);
    cancel.push_back(uint8_t(TargetType::Cancel));
    cancel.resize(19, 0);
    REQUIRE(f.fixed(cancel));
    CHECK_FALSE(f.world.target.isTargeting);
    CHECK(f.events.targetChanges == 2);
}

TEST_CASE("0x3A loads the full skill list")
{
    Fixture f;
    f.login();

    Bytes p = var(0x3A);
    p.push_back(0x00); // full list without caps, 1-based ids
    be16(p, 1), be16(p, 505), be16(p, 500), p.push_back(0);                     // alchemy
    be16(p, 26), be16(p, 1000), be16(p, 1000), p.push_back(uint8_t(SkillLock::Locked)); // magery
    be16(p, 0);
    REQUIRE(f.variable(finish(p)));

    const auto& skills = f.world.player()->skills;
    REQUIRE(skills.size() >= 26);
    CHECK(skills[0].valueFixed == 505);
    CHECK(skills[0].baseFixed == 500);
    CHECK(skills[25].valueFixed == 1000);
    CHECK(skills[25].lock == SkillLock::Locked);
    CHECK(skills[25].capFixed == 1000);
    CHECK(f.events.skillsChanged == 1);
}

TEST_CASE("0xAF re-keys the dying mobile and records its corpse")
{
    Fixture f;
    f.login();
    f.spawnOrc();

    Bytes p{0xAF};
    be32(p, kOrc);
    be32(p, 0x40000600);
    be32(p, 0);
    REQUIRE(f.fixed(p));

    const Serial dead = kOrc | 0x80000000u;
    CHECK(f.world.mobile(kOrc) == nullptr);
    REQUIRE(f.world.mobile(dead) != nullptr);
    CHECK(f.world.item(0x40000200)->container == dead);
    CHECK(f.world.corpseExists(0x40000600, dead));
    REQUIRE(f.events.deaths.size() == 1);
    CHECK(f.events.deaths[0] == std::pair<Serial, Serial>{dead, 0x40000600});
}

TEST_CASE("0xBF sub-commands: party and map change")
{
    Fixture f;
    f.login();
    f.spawnOrc(); // an ally, for the party list

    Bytes party = var(0xBF);
    be16(party, 0x06);
    party.push_back(1); // add members
    party.push_back(2);
    be32(party, kPlayer);
    be32(party, kOrc);
    REQUIRE(f.variable(finish(party)));
    CHECK(f.world.party.leader == kPlayer);
    CHECK(f.world.party.contains(kOrc));
    CHECK(f.events.partyChanges == 1);

    Bytes map = var(0xBF);
    be16(map, 0x08);
    map.push_back(1); // Trammel
    REQUIRE(f.variable(finish(map)));
    CHECK(f.world.mapIndex == 1);
    CHECK(f.events.mapChanges.back() == 1);
    // The player survives a map change; everything else in the world does not.
    CHECK(f.world.player() != nullptr);
    CHECK(f.world.mobile(kOrc) == nullptr);
}

TEST_CASE("0x6F opens a secure trade")
{
    Fixture f;
    f.login();
    f.spawnOrc();

    // Trade boxes arrive as equipment on each side first.
    for (auto [box, owner] : {std::pair{0x40000700u, kPlayer}, std::pair{0x40000701u, kOrc}})
    {
        Bytes e{0x2E};
        be32(e, box);
        be16(e, 0x1E5E), e.push_back(0), e.push_back(uint8_t(Layer::Invalid));
        be32(e, owner), be16(e, 0);
        REQUIRE(f.fixed(e));
    }

    Bytes p = var(0x6F);
    p.push_back(0);
    be32(p, kOrc);
    be32(p, 0x40000700);
    be32(p, 0x40000701);
    p.push_back(1);
    ascii(p, "an orc", 7);
    REQUIRE(f.variable(finish(p)));

    REQUIRE(f.events.trades.size() == 1);
    CHECK(f.events.trades[0].kind == TradeEvent::Kind::Open);
    CHECK(f.events.trades[0].myBox == 0x40000700);
    CHECK(f.events.trades[0].theirBox == 0x40000701);
    CHECK(f.events.trades[0].theirName == "an orc");
}

TEST_CASE("0x74 prices a vendor's buy box")
{
    Fixture f;
    f.login();
    f.spawnOrc(); // stands in for a vendor

    constexpr Serial box = 0x40000800;
    Bytes e{0x2E};
    be32(e, box);
    be16(e, 0x0E75), e.push_back(0), e.push_back(uint8_t(Layer::ShopBuy));
    be32(e, kOrc), be16(e, 0);
    REQUIRE(f.fixed(e));

    Bytes list = var(0x3C);
    be16(list, 2);
    for (auto b : f.containedItem(0x40000801, 0x0F0E, 20, 0, 0, box))
        list.push_back(b);
    for (auto b : f.containedItem(0x40000802, 0x0F7A, 50, 0, 0, box))
        list.push_back(b);
    REQUIRE(f.variable(finish(list)));

    // Prices come in reverse order of the box contents.
    Bytes p = var(0x74);
    be32(p, box);
    p.push_back(2);
    be32(p, 5), p.push_back(12), ascii(p, "black pearl", 12);
    be32(p, 3), p.push_back(13), ascii(p, "empty bottle", 13);
    REQUIRE(f.variable(finish(p)));

    CHECK(f.world.item(0x40000802)->price == 5);
    CHECK(f.world.item(0x40000802)->name == "black pearl");
    CHECK(f.world.item(0x40000801)->price == 3);
    CHECK(f.world.item(0x40000801)->name == "empty bottle");
}

TEST_CASE("gump and walk packets are left to their own modules")
{
    Fixture f;
    f.login();

    CHECK_FALSE(f.handlers.handles(0xB0));
    CHECK_FALSE(f.handlers.handles(0xDD));
    CHECK_FALSE(f.handlers.handles(0x22));

    int gumps = 0;
    f.handlers.setHook(0xB0, [&](World&, io::BinaryReader& r) {
        CHECK(r.readU32BE() == kPlayer);
        ++gumps;
    });
    Bytes gump = var(0xB0);
    be32(gump, kPlayer);
    CHECK(f.variable(finish(gump)));
    CHECK(gumps == 1);

    uint32_t key = 0;
    f.handlers.setExtendedHook(0x02, [&](World&, io::BinaryReader& r) { key = r.readU32BE(); });
    Bytes fastWalk = var(0xBF);
    be16(fastWalk, 0x02);
    be32(fastWalk, 0xCAFEBABE);
    CHECK(f.variable(finish(fastWalk)));
    CHECK(key == 0xCAFEBABE);
}

TEST_CASE("the packet table picks the header size")
{
    Fixture f;
    net::PacketTable table(f.world.clientVersion);
    REQUIRE(f.handlers.handle(f.world, enterWorld(kPlayer, 5, 6), table));

    Bytes hits{0xA1};
    be32(hits, kPlayer), be16(hits, 50), be16(hits, 49);
    REQUIRE(f.handlers.handle(f.world, hits, table));
    CHECK(f.world.player()->hits == 49);

    Bytes status = var(0x11);
    be32(status, kPlayer);
    ascii(status, "Brett", 30);
    be16(status, 48), be16(status, 50);
    status.push_back(0), status.push_back(0);
    REQUIRE(f.handlers.handle(f.world, finish(status), table));
    CHECK(f.world.player()->name == "Brett");
    CHECK(f.world.player()->hits == 48);
}
