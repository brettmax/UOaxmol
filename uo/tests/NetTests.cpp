// SPDX-License-Identifier: BSD-2-Clause
#include "TestUtil.h"
#include "doctest.h"

#include "uo/game/World.h"
#include "uo/net/Huffman.h"
#include "uo/net/OutgoingPackets.h"
#include "uo/net/PacketFramer.h"
#include "uo/net/PacketTable.h"
#include "uo/net/Session.h"

#include <deque>

using namespace uo;
using namespace uo::net;
using namespace uotest;

TEST_CASE("huffman round-trips every byte value, across arbitrary read boundaries")
{
    Bytes packet;
    for (int i = 0; i < 256; ++i)
        packet.push_back(static_cast<std::uint8_t>(i));

    Bytes wire;
    HuffmanEncoder::encode(packet, wire);
    HuffmanEncoder::encode(Bytes{0x73, 0x01}, wire);

    for (std::size_t chunk : {std::size_t(1), std::size_t(3), std::size_t(7), wire.size()})
    {
        HuffmanDecoder d;
        Bytes out;
        for (std::size_t at = 0; at < wire.size(); at += chunk)
            d.decode(std::span<const std::uint8_t>(wire).subspan(at, std::min(chunk, wire.size() - at)), out);
        Bytes expected = packet;
        expected.push_back(0x73);
        expected.push_back(0x01);
        CHECK(out == expected);
    }
}

TEST_CASE("packet table is calibrated by client version")
{
    PacketTable t2a(cv::CV_200);
    PacketTable modern(makeVersion(7, 0, 15, 1));
    CHECK(t2a.length(0x80) == 62);
    CHECK(t2a.length(0x0B) == 0x10A);
    CHECK(modern.length(0x0B) == 7);
    CHECK(t2a.length(0xB9) == 3);
    CHECK(modern.length(0xB9) == 5);
    CHECK(modern.length(0xF3) == 0x1A);
    CHECK(modern.length(0xA8) == -1);
    CHECK(modern.length(0xFF) == -1);
}

TEST_CASE("outgoing packets have the sizes the server expects")
{
    PacketTable t(makeVersion(7, 0, 15, 1));
    for (auto& p : {out::accountLogin("a", "b"), out::selectShard(0), out::gameLogin(1, "a", "b"),
                    out::selectCharacter(0, "x", 0, 0), out::walkRequest(1, 0), out::ping(0),
                    out::doubleClick(1), out::seed(1, makeVersion(7, 0, 15, 1))})
    {
        CAPTURE(int(p[0]));
        CHECK(static_cast<int>(p.size()) == t.length(p[0]));
    }
    auto v = out::clientVersion("7.0.15.1");
    CHECK(v.size() == 3 + 9);
    CHECK(((v[1] << 8) | v[2]) == 12);

    auto s = out::speech("hi", 0, 0x3B2, 3);
    CHECK(s.size() == 3 + 1 + 2 + 2 + 4 + 6);
}

TEST_CASE("framer splits fixed and variable packets across reads")
{
    PacketTable t(makeVersion(7, 0, 15, 1));
    PacketFramer f(t);
    std::vector<Bytes> got;
    auto collect = [&](std::span<const std::uint8_t> p) { got.emplace_back(p.begin(), p.end()); };

    Bytes stream = out::ping(9);
    Bytes var    = out::clientVersion("abc");
    stream.insert(stream.end(), var.begin(), var.end());

    for (std::uint8_t b : stream)
        REQUIRE(f.feed(std::span<const std::uint8_t>(&b, 1), collect));
    REQUIRE(got.size() == 2);
    CHECK(got[0] == out::ping(9));
    CHECK(got[1] == var);

    Bytes bad{0xBD, 0x00, 0x01};
    CHECK_FALSE(f.feed(bad, collect));
}

namespace
{
// Plays the login and game servers against a Session.
struct FakeTransport : Transport
{
    std::vector<std::pair<std::string, std::uint16_t>> connects;
    std::vector<Bytes> sent;
    int disconnects = 0;
    Session* session = nullptr;

    void connect(const std::string& host, std::uint16_t port) override { connects.emplace_back(host, port); }
    void send(Bytes b) override { sent.push_back(std::move(b)); }
    void disconnect() override
    {
        ++disconnects;
        if (session)
            session->onDisconnected();
    }
};

struct Recorder : SessionListener
{
    std::vector<ShardInfo> shards;
    std::vector<CharacterSlot> characters;
    std::vector<Bytes> game;
    std::string error;
    game::World world;

    void onShardList(const std::vector<ShardInfo>& s) override { shards = s; }
    void onCharacterList(const std::vector<CharacterSlot>& c) override { characters = c; }
    void onLoginError(std::string m) override { error = std::move(m); }
    void onGamePacket(std::span<const std::uint8_t> p) override
    {
        game.emplace_back(p.begin(), p.end());
        world.handle(p);
    }
};

Bytes compressed(const Bytes& packet)
{
    Bytes out;
    HuffmanEncoder::encode(packet, out);
    return out;
}
}  // namespace

TEST_CASE("session walks the full login flow into the world")
{
    FakeTransport net;
    Recorder rec;
    Session s(net, rec);
    net.session = &s;

    Session::Settings cfg;
    cfg.host     = "shard.example";
    cfg.port     = 2593;
    cfg.account  = "brett";
    cfg.password = "secret";
    s.start(cfg);
    REQUIRE(net.connects.size() == 1);
    CHECK(net.connects[0].first == "shard.example");

    s.onConnected();
    REQUIRE(net.sent.size() == 2);
    CHECK(net.sent[0][0] == 0xEF);
    CHECK(net.sent[1][0] == 0x80);
    CHECK(s.state() == Session::State::LoginAuthenticating);

    // 0xA8 with one shard.
    Bytes a8{0xA8, 0, 0, 0x5D};
    be16(a8, 1);
    be16(a8, 0);
    ascii(a8, "ModernUO", 32);
    a8.push_back(0), a8.push_back(0);
    be32(a8, 0x0100007F);
    a8[1] = static_cast<std::uint8_t>(a8.size() >> 8), a8[2] = static_cast<std::uint8_t>(a8.size());
    s.onData(a8);
    REQUIRE(rec.shards.size() == 1);
    CHECK(rec.shards[0].name == "ModernUO");

    s.selectShard(0);
    CHECK(net.sent.back() == out::selectShard(0));

    // 0x8C: relay to 10.0.0.5:2594 with key 0xCAFEBABE.
    Bytes relay{0x8C, 10, 0, 0, 5};
    be16(relay, 2594);
    be32(relay, 0xCAFEBABE);
    s.onData(relay);
    REQUIRE(net.connects.size() == 2);
    CHECK(net.connects[1].first == "10.0.0.5");
    CHECK(net.connects[1].second == 2594);
    CHECK(s.state() == Session::State::GameConnecting);
    CHECK(rec.error.empty());

    std::size_t before = net.sent.size();
    s.onConnected();
    REQUIRE(net.sent.size() == before + 2);
    CHECK(net.sent[before] == Bytes{0xCA, 0xFE, 0xBA, 0xBE});
    CHECK(net.sent[before + 1] == out::gameLogin(0xCAFEBABE, "brett", "secret"));

    // From here on the server compresses. Character list with an empty slot 0 and "Lord British" in slot 1.
    Bytes a9{0xA9, 0, 0, 2};
    ascii(a9, "", 60);
    ascii(a9, "Lord British", 30);
    ascii(a9, "", 30);
    a9.push_back(0);  // city count
    be32(a9, 0);      // flags
    a9[1] = static_cast<std::uint8_t>(a9.size() >> 8), a9[2] = static_cast<std::uint8_t>(a9.size());
    s.onData(compressed(a9));
    REQUIRE(rec.characters.size() == 1);
    CHECK(rec.characters[0].slot == 1);
    CHECK(rec.characters[0].name == "Lord British");

    s.selectCharacter(1);
    CHECK(net.sent.back()[0] == 0x5D);
    CHECK(net.sent.back()[5] == 'L');

    // 0x1B login confirm at Britain bank.
    Bytes b1b{0x1B};
    be32(b1b, 0x00001234);
    be32(b1b, 0);
    be16(b1b, 0x0190);
    be16(b1b, 1434), be16(b1b, 1690), be16(b1b, 20);
    b1b.push_back(2);
    b1b.resize(37, 0);
    // 0xBD version request arrives in the same read.
    Bytes wire = compressed(b1b);
    Bytes bd   = compressed(Bytes{0xBD, 0x00, 0x03});
    wire.insert(wire.end(), bd.begin(), bd.end());
    s.onData(wire);

    CHECK(s.state() == Session::State::InGame);
    CHECK(net.sent.back() == out::clientVersion("7.0.15.1"));
    REQUIRE(rec.world.player());
    CHECK(rec.world.player()->x == 1434);
    CHECK(rec.world.player()->y == 1690);
    CHECK(rec.world.player()->z == 20);
    CHECK(rec.world.player()->graphic == 0x0190);
}

TEST_CASE("login denial surfaces a readable error")
{
    FakeTransport net;
    Recorder rec;
    Session s(net, rec);
    s.start({"h", 2593, "a", "b"});
    s.onConnected();
    s.onData(Bytes{0x82, 0x00});
    CHECK(rec.error == "Incorrect name or password.");
}
