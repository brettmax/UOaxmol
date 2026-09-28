// SPDX-License-Identifier: BSD-2-Clause
#include "uo/game/TargetCursor.h"

#include "doctest.h"

#include <vector>

using namespace uo::game;

namespace
{

struct Harness
{
    std::vector<std::vector<uint8_t>> sent;
    std::vector<uint32_t> statusRequests;
    int multiCleared = 0;
    bool answerQuery = true;
    std::function<void()> pendingQuery;

    TargetCursor make()
    {
        TargetCursorHooks hooks;
        hooks.send = [this](std::span<const uint8_t> b) { sent.emplace_back(b.begin(), b.end()); };
        hooks.requestMobileStatus = [this](uint32_t s) { statusRequests.push_back(s); };
        hooks.clearMultiPreview = [this] { multiCleared++; };
        hooks.askCriminalAction = [this](std::function<void()> proceed) {
            pendingQuery = std::move(proceed);
            return answerQuery;
        };
        return TargetCursor(std::move(hooks));
    }
};

std::vector<uint8_t> serverTarget(uint8_t state, uint32_t cursor, uint8_t type)
{
    std::vector<uint8_t> p{0x6C, state, static_cast<uint8_t>(cursor >> 24), static_cast<uint8_t>(cursor >> 16),
                           static_cast<uint8_t>(cursor >> 8), static_cast<uint8_t>(cursor), type};
    p.resize(19, 0);
    return p;
}

}  // namespace

TEST_CASE("targeting: server cursor, object target and packet layout")
{
    Harness h;
    TargetCursor tc = h.make();

    REQUIRE(tc.handleServerTarget(serverTarget(0, 0x01020304, 1)));
    CHECK(tc.isTargeting());
    CHECK(tc.state() == CursorTarget::Object);
    CHECK(tc.type() == TargetType::Harmful);

    TargetEntity orc{0x00001234, 0x0011, 1500, 1600, -3, 1, Notoriety::Enemy, false};
    tc.target(orc, Notoriety::Innocent, TargetOptions{});

    REQUIRE(h.sent.size() == 1);
    const std::vector<uint8_t> expect{0x6C, 0x00, 0x01, 0x02, 0x03, 0x04, 0x01, 0x00, 0x00, 0x12,
                                      0x34, 0x05, 0xDC, 0x06, 0x40, 0xFF, 0xFD, 0x00, 0x11};
    CHECK(h.sent[0] == expect);
    CHECK_FALSE(tc.isTargeting());
    CHECK(tc.lastTarget().serial == 0x1234);
    CHECK(tc.lastTarget().isEntity());
}

TEST_CASE("targeting: land and statics send position targets")
{
    Harness h;
    TargetCursor tc = h.make();

    // Object cursors ignore land.
    tc.setTargeting(CursorTarget::Object, 7, TargetType::Neutral);
    tc.target(0, 100, 200, 5, 0, TargetOptions{});
    CHECK(h.sent.empty());
    CHECK(tc.isTargeting());

    tc.setTargeting(CursorTarget::Position, 7, TargetType::Neutral);
    tc.target(0x0B10, 100, 200, 5, 6, TargetOptions{});  // a 6-high surface: target its top
    REQUIRE(h.sent.size() == 1);
    CHECK(h.sent[0][1] == 0x01);
    CHECK(h.sent[0][16] == 11);
    CHECK((h.sent[0][17] == 0x0B && h.sent[0][18] == 0x10));
    CHECK(tc.lastTarget().isStatic());

    // Target-last replays the body under the new cursor.
    tc.setTargeting(CursorTarget::Position, 9, TargetType::Beneficial);
    tc.targetLast();
    REQUIRE(h.sent.size() == 2);
    CHECK(h.sent[1][5] == 9);
    CHECK(h.sent[1][6] == 2);
    CHECK(h.sent[1][16] == 11);
}

TEST_CASE("targeting: cancel, and server cancel answers with the old cursor")
{
    Harness h;
    TargetCursor tc = h.make();

    tc.setTargeting(CursorTarget::Object, 5, TargetType::Neutral);
    tc.cancel();
    REQUIRE(h.sent.size() == 1);
    CHECK(h.sent[0] == std::vector<uint8_t>{0x6C, 0x00, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0xFF, 0xFF, 0xFF, 0xFF, 0, 0, 0, 0});
    CHECK_FALSE(tc.isTargeting());

    h.sent.clear();
    tc.setTargeting(CursorTarget::Object, 5, TargetType::Neutral);
    tc.handleServerTarget(serverTarget(0, 9, 3));
    REQUIRE(h.sent.size() == 1);
    CHECK(h.sent[0][5] == 5);
    CHECK(h.sent[0][6] == 3);
    CHECK_FALSE(tc.isTargeting());
    CHECK(tc.cursorId() == 9);
}

TEST_CASE("targeting: harmful action on an innocent asks first")
{
    Harness h;
    TargetCursor tc = h.make();
    tc.setTargeting(CursorTarget::Object, 1, TargetType::Harmful);

    TargetEntity blue{0x00000077, 0x0190, 10, 10, 0, 1, Notoriety::Innocent, false};
    tc.target(blue, Notoriety::Innocent, TargetOptions{});
    CHECK(h.sent.empty());
    REQUIRE(h.pendingQuery);

    h.pendingQuery();
    CHECK(h.sent.size() == 1);
    CHECK_FALSE(tc.isTargeting());

    // A query already open: target straight away.
    h.sent.clear();
    h.answerQuery = false;
    tc.setTargeting(CursorTarget::Object, 1, TargetType::Harmful);
    tc.target(blue, Notoriety::Innocent, TargetOptions{});
    CHECK(h.sent.size() == 1);
}

TEST_CASE("targeting: multi placement and callback cursors")
{
    Harness h;
    TargetCursor tc = h.make();

    // The 26-byte form older clients get has no hue; the 30-byte form does.
    std::vector<uint8_t> p(26, 0);
    p[0] = 0x99;
    p[5] = 0x2A;   // deed serial low byte
    p[19] = 0x64;  // model
    REQUIRE(tc.handleMultiPlacement(p));
    CHECK(tc.multi()->hue == 0);

    p.resize(30, 0);
    p[26] = 0x04;
    p[27] = 0x55;  // hue
    REQUIRE(tc.handleMultiPlacement(p));
    CHECK(tc.state() == CursorTarget::MultiPlacement);
    REQUIRE(tc.multi().has_value());
    CHECK(tc.multi()->model == 0x64);
    CHECK(tc.multi()->hue == 0x0455);

    tc.sendMultiTarget(1000, 1000, 0);
    CHECK(h.sent.size() == 1);
    CHECK_FALSE(tc.multi().has_value());
    CHECK(h.multiCleared == 1);

    std::optional<uint32_t> picked;
    bool cancelled = false;
    tc.setTargeting([&](const TargetCursor::Picked* p) {
        if (p == nullptr)
        {
            cancelled = true;
        }
        else
        {
            picked = p->serial;
        }
    }, 0, TargetType::Neutral);
    tc.target(TargetEntity{0x40000001}, Notoriety::Innocent, TargetOptions{});
    CHECK(picked == 0x40000001u);

    tc.setTargeting([&](const TargetCursor::Picked* p) { cancelled = p == nullptr; }, 0, TargetType::Neutral);
    tc.cancel();
    CHECK(cancelled);
}
