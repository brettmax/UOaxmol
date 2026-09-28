// SPDX-License-Identifier: BSD-2-Clause
#include "uo/movement/AutoWalker.h"
#include "uo/movement/Direction.h"
#include "uo/movement/MobileMotion.h"
#include "uo/movement/MovementInput.h"
#include "uo/movement/MovementPackets.h"
#include "uo/movement/Pathfinder.h"
#include "uo/movement/Walker.h"

#include "doctest.h"

#include <map>
#include <utility>

using namespace uo::movement;

namespace
{

// Flat walkable land at z 0 everywhere, plus whatever statics a test places.
class FakeTiles : public ITileSource
{
public:
    bool gatherTile(int x, int y, std::vector<TileObject>& out) override
    {
        if (x < 0 || y < 0)
        {
            return false;
        }

        TileObject land;
        land.kind = TileObjectKind::Land;
        land.graphic = 0x0003;  // grass
        land.z = 0;
        out.push_back(land);

        auto [begin, end] = statics.equal_range({x, y});
        for (auto it = begin; it != end; ++it)
        {
            out.push_back(it->second);
        }
        return true;
    }

    void wall(int x, int y)
    {
        TileObject s;
        s.kind = TileObjectKind::Static;
        s.graphic = 0x0080;
        s.tileFlags = tile_flag::Impassable;
        s.height = 20;
        statics.emplace(std::pair{x, y}, s);
    }

    void place(int x, int y, uint64_t flags, uint8_t height, int8_t z = 0)
    {
        TileObject s;
        s.kind = TileObjectKind::Static;
        s.graphic = 0x0100;
        s.tileFlags = flags;
        s.height = height;
        s.z = z;
        statics.emplace(std::pair{x, y}, s);
    }

    std::multimap<std::pair<int, int>, TileObject> statics;
};

PlayerWalkState playerAt(int x, int y, Direction facing)
{
    PlayerWalkState p;
    p.x = x;
    p.y = y;
    p.facing = facing;
    p.stamina = 50;
    return p;
}

}  // namespace

TEST_CASE("movement: direction helpers match ClassicUO")
{
    CHECK(directionFromArrows(true, false, false, false) == Direction::Up);
    CHECK(directionFromArrows(true, false, false, true) == Direction::North);
    CHECK(directionFromArrows(false, true, true, false) == Direction::South);
    CHECK(directionFromArrows(false, false, false, true) == Direction::Right);
    CHECK(directionFromArrows(false, false, false, false) == kDirectionNone);

    // Screen up is world north-west, screen right is world north-east.
    CHECK(mouseWalkDirection(400, 300, 400, 100) == Direction::Up);
    CHECK(mouseWalkDirection(400, 300, 700, 300) == Direction::Right);
    CHECK(mouseWalkDirection(400, 300, 400, 500) == Direction::Down);
    CHECK(mouseWalkDirection(400, 300, 100, 300) == Direction::Left);
    CHECK(mouseWalkDirection(400, 300, 600, 100) == Direction::North);

    CHECK(directionOfDelta(10, 10, 10, 9) == Direction::North);
    CHECK(directionOfDelta(10, 10, 11, 11) == Direction::Down);
    CHECK(directionOfDelta(10, 10, 10, 10) == kDirectionNone);
    CHECK(reverse(Direction::North) == Direction::South);

    int x = 5, y = 5;
    offsetXY(Direction::Up, x, y);
    CHECK((x == 4 && y == 4));
}

TEST_CASE("movement: canWalk on flat land, walls, and blocked diagonals")
{
    FakeTiles tiles;
    Pathfinder pf(tiles);

    Direction dir = Direction::North;
    int x = 10, y = 10;
    int8_t z = 0;
    CHECK(pf.canWalk(dir, x, y, z));
    CHECK((x == 10 && y == 9 && z == 0 && dir == Direction::North));

    tiles.wall(10, 8);
    dir = Direction::North;
    CHECK_FALSE(pf.canWalk(dir, x, y, z));
    CHECK((x == 10 && y == 9));

    // North-east from (10,9) needs (10,8) and (11,9) open; (10,8) is walled, so it slides east.
    dir = Direction::Right;
    CHECK(pf.canWalk(dir, x, y, z));
    CHECK(dir == Direction::East);
    CHECK((x == 11 && y == 9));
}

TEST_CASE("movement: surfaces and stairs resolve z")
{
    FakeTiles tiles;
    Pathfinder pf(tiles);

    // A 5-high table is too tall to step onto from the floor.
    tiles.place(10, 9, tile_flag::Surface, 5);
    Direction dir = Direction::North;
    int x = 10, y = 10;
    int8_t z = 0;
    CHECK_FALSE(pf.canWalk(dir, x, y, z));

    // A stair (bridge) of the same height is: it stands at half its height.
    tiles.place(11, 9, tile_flag::Surface | tile_flag::Bridge, 5);
    dir = Direction::North;
    x = 11;
    y = 10;
    z = 0;
    CHECK(pf.canWalk(dir, x, y, z));
    CHECK(z == 2);
}

TEST_CASE("movement: mobiles block unless characters are ignored")
{
    FakeTiles tiles;
    TileObject mob;
    mob.kind = TileObjectKind::Mobile;
    mob.z = 0;
    tiles.statics.emplace(std::pair{10, 9}, mob);

    Pathfinder pf(tiles);
    WalkerContext ctx;
    pf.setContext(ctx);

    Direction dir = Direction::North;
    int x = 10, y = 10;
    int8_t z = 0;
    CHECK_FALSE(pf.canWalk(dir, x, y, z));

    ctx.ignoreCharacters = WalkerContext::ignoresCharacters(false, StepState::Normal, false, 50, 50, 0);
    pf.setContext(ctx);
    CHECK(pf.canWalk(dir, x, y, z));
    CHECK(WalkerContext::ignoresCharacters(false, StepState::Normal, false, 40, 50, 0) == false);
}

TEST_CASE("movement: A* routes around a wall")
{
    FakeTiles tiles;
    for (int wx = 5; wx <= 15; wx++)
    {
        tiles.wall(wx, 8);
    }

    Pathfinder pf(tiles);
    REQUIRE(pf.findPath(10, 10, 0, 10, 5, 0, 0));

    const auto& path = pf.path();
    REQUIRE(path.size() > 2);
    CHECK((path.front().x == 10 && path.front().y == 10));
    CHECK((path.back().x == 10 && path.back().y == 5));

    for (size_t i = 1; i < path.size(); i++)
    {
        int px = path[i - 1].x, py = path[i - 1].y;
        offsetXY(path[i].direction, px, py);
        CHECK((px == path[i].x && py == path[i].y));
        CHECK_FALSE((path[i].y == 8 && path[i].x >= 5 && path[i].x <= 15));
    }
}

TEST_CASE("movement: A* goal on a blocked tile stops next to it")
{
    FakeTiles tiles;
    tiles.wall(10, 5);

    Pathfinder pf(tiles);
    REQUIRE(pf.findPath(10, 10, 0, 10, 5, 0, 0));
    const PathStep& last = pf.path().back();
    CHECK(std::max(std::abs(last.x - 10), std::abs(last.y - 5)) == 1);
}

TEST_CASE("movement: walker steps, turns, sequences and fast-walk keys")
{
    FakeTiles tiles;
    Pathfinder pf(tiles);
    Walker walker;
    WalkOptions opts;

    walker.fastWalk().set(0, 0x11111111);
    walker.fastWalk().set(1, 0x22222222);

    // Facing north and asking north: a step.
    WalkResult r = walker.walk(Direction::North, false, playerAt(10, 10, Direction::North), opts, pf, 1000);
    REQUIRE(r.kind == WalkResult::Kind::Sent);
    CHECK_FALSE(r.turnOnly);
    CHECK((r.x == 10 && r.y == 9));
    CHECK(r.walkTime == kStepDelayWalk);
    CHECK(r.packet == std::vector<uint8_t>{0x02, 0x00, 0x00, 0x11, 0x11, 0x11, 0x11});
    CHECK(walker.nextSequence() == 1);

    // Too soon for the next one.
    PlayerWalkState p = playerAt(10, 10, Direction::North);
    p.hasQueuedSteps = true;
    p.x = 10;
    p.y = 9;
    p.lastQueuedDirection = Direction::North;
    CHECK(walker.walk(Direction::North, false, p, opts, pf, 1100).kind == WalkResult::Kind::Rejected);

    // Turning east costs only the turn delay and does not move; running sets bit 0x80.
    r = walker.walk(Direction::East, true, p, opts, pf, 1400);
    REQUIRE(r.kind == WalkResult::Kind::Sent);
    CHECK(r.turnOnly);
    CHECK((r.x == 10 && r.y == 9));
    CHECK(r.walkTime == kTurnDelay);
    CHECK(r.packet == std::vector<uint8_t>{0x02, 0x82, 0x01, 0x22, 0x22, 0x22, 0x22});

    // Out of stamina: never runs.
    p.lastQueuedDirection = Direction::East;
    p.stamina = 1;
    r = walker.walk(Direction::East, true, p, opts, pf, 2000);
    REQUIRE(r.kind == WalkResult::Kind::Sent);
    CHECK_FALSE(r.run);
    CHECK(r.packet[1] == 0x02);
    CHECK(r.packet[3] == 0);  // key stack empty
}

TEST_CASE("movement: walker window, coalescing, confirm and deny")
{
    FakeTiles tiles;
    Pathfinder pf(tiles);
    Walker walker;
    WalkOptions opts;

    PlayerWalkState p = playerAt(100, 100, Direction::North);
    uint64_t now = 0;

    // Three unconfirmed steps.
    for (int i = 0; i < 3; i++)
    {
        now += 1000;
        WalkResult r = walker.walk(Direction::North, false, p, opts, pf, now);
        REQUIRE(r.kind == WalkResult::Kind::Sent);
        p.hasQueuedSteps = true;
        p.x = r.x;
        p.y = r.y;
        p.lastQueuedDirection = r.direction;
    }
    CHECK(walker.unacceptedCount() == 3);

    // With three in flight, a turn is folded into the facing without a packet.
    now += 1000;
    WalkResult turn = walker.walk(Direction::West, false, p, opts, pf, now);
    CHECK(turn.kind == WalkResult::Kind::Coalesced);
    CHECK(turn.packet.empty());

    // Two more steps fill the window of five.
    for (int i = 0; i < 2; i++)
    {
        now += 1000;
        WalkResult r = walker.walk(Direction::North, false, p, opts, pf, now);
        REQUIRE(r.kind == WalkResult::Kind::Sent);
        p.x = r.x;
        p.y = r.y;
    }
    now += 1000;
    CHECK(walker.walk(Direction::North, false, p, opts, pf, now).kind == WalkResult::Kind::Rejected);

    // The server accepts sequence 0.
    ConfirmResult c = walker.confirm(0);
    CHECK_FALSE(c.badStep);
    REQUIRE(c.rangeCenter.has_value());
    CHECK(c.rangeCenter->second == 99);
    CHECK(walker.step(0).accepted);

    // Its animation finishing retires it and frees a slot.
    walker.onStepAnimated();
    CHECK(walker.stepsCount() == 4);

    // A sequence we never sent is a bad step: resync once, stop walking until the server resets us.
    c = walker.confirm(0x77);
    CHECK(c.badStep);
    CHECK(c.sendResync);
    CHECK(walker.walkingFailed());
    CHECK_FALSE(walker.confirm(0x78).sendResync);
    CHECK(walker.walk(Direction::North, false, p, opts, pf, now + 5000).kind == WalkResult::Kind::Rejected);

    walker.deny(Direction::East);
    CHECK_FALSE(walker.walkingFailed());
    CHECK(walker.stepsCount() == 0);
    CHECK(walker.nextSequence() == 0);
    CHECK(walker.serverDirection() == Direction::East);
}

TEST_CASE("movement: walk sequence wraps from 255 to 1")
{
    FakeTiles tiles;
    Pathfinder pf(tiles);
    Walker walker;
    WalkOptions opts;
    PlayerWalkState p = playerAt(100, 100, Direction::North);

    uint64_t now = 0;
    for (int i = 0; i < 256; i++)
    {
        now += 1000;
        WalkResult r = walker.walk(Direction::North, false, p, opts, pf, now);
        REQUIRE(r.kind == WalkResult::Kind::Sent);
        CHECK(r.sequence == (i == 0 ? 0 : ((i - 1) % 255) + 1));
        walker.confirm(r.sequence);
        walker.onStepAnimated();
    }
}

TEST_CASE("movement: packets parse")
{
    const std::vector<uint8_t> deny{0x21, 0x05, 0x01, 0x00, 0x02, 0x00, 0x86, 0xFB};
    auto d = packets::parseDenyWalk(deny);
    REQUIRE(d.has_value());
    CHECK(d->sequence == 5);
    CHECK((d->x == 256 && d->y == 512));
    CHECK(d->direction == Direction::West);
    CHECK(d->z == -5);

    const std::vector<uint8_t> confirm{0x22, 0x07, 0x46};
    auto c = packets::parseConfirmWalk(confirm);
    REQUIRE(c.has_value());
    CHECK(c->sequence == 7);
    CHECK(c->notoriety == 6);
    CHECK(packets::parseConfirmWalk(std::vector<uint8_t>{0x22, 0x07, 0x00})->notoriety == 1);
    CHECK_FALSE(packets::parseConfirmWalk(std::vector<uint8_t>{0x22, 0x07}).has_value());

    auto m = packets::parseMovePlayer(std::vector<uint8_t>{0x97, 0x84});
    REQUIRE(m.has_value());
    CHECK(masked(*m) == Direction::South);
    CHECK(isRunning(*m));

    std::vector<uint8_t> keys{0xBF, 0x00, 0x1D, 0x00, 0x01};
    for (uint8_t i = 1; i <= 6; i++)
    {
        keys.insert(keys.end(), {0, 0, 0, i});
    }
    auto k = packets::parseFastWalkKeys(keys);
    REQUIRE(k.has_value());
    CHECK(k->replace);
    CHECK(k->keys.size() == 6);
    CHECK(k->keys[5] == 6);

    auto add = packets::parseFastWalkKeys(std::vector<uint8_t>{0xBF, 0x00, 0x09, 0x00, 0x02, 0xDE, 0xAD, 0xBE, 0xEF});
    REQUIRE(add.has_value());
    CHECK_FALSE(add->replace);
    CHECK(add->keys[0] == 0xDEADBEEF);

    FastWalkStack stack;
    for (size_t i = 0; i < k->keys.size(); i++)
    {
        stack.set(static_cast<int>(i), k->keys[i]);  // the sixth key is dropped
    }
    for (uint32_t expect = 1; expect <= 5; expect++)
    {
        CHECK(stack.take() == expect);
    }
    CHECK(stack.take() == 0);
}

TEST_CASE("movement: auto-walk turns then steps along the path")
{
    FakeTiles tiles;
    Pathfinder pf(tiles);
    AutoWalker auto_;

    REQUIRE(auto_.start(pf, 10, 10, 0, 13, 10, 0, 0));
    CHECK(auto_.active());

    // Facing north, the first node is east: the request turns and stays on that node.
    auto r = auto_.tick(Direction::North, true);
    REQUIRE(r.has_value());
    CHECK(r->direction == Direction::East);

    // Now facing east: steps, three times, then stops.
    int requests = 0;
    while (auto next = auto_.tick(Direction::East, true))
    {
        CHECK(next->direction == Direction::East);
        requests++;
    }
    CHECK(requests == 3);
    CHECK_FALSE(auto_.active());

    // Waiting on the walker yields nothing.
    REQUIRE(auto_.start(pf, 10, 10, 0, 13, 10, 0, 0));
    CHECK_FALSE(auto_.tick(Direction::East, false).has_value());
}

TEST_CASE("movement: mobile motion interpolates and completes steps")
{
    MobileMotion m;
    m.x = 10;
    m.y = 10;
    m.direction = 0;
    m.lastStepTime = 1000;
    m.steps.push_back({10, 9, 0, 0, false});

    // Halfway through a 400 ms walk the mobile is drawn between tiles.
    MotionUpdate u = advanceMotion(m, nullptr, false, 1200, 0);
    CHECK(u.stepsCompleted == 0);
    CHECK(m.offsetX != 0);
    CHECK(m.offsetY != 0);

    u = advanceMotion(m, nullptr, false, 1400, 0);
    CHECK(u.stepsCompleted == 1);
    CHECK(u.tileChanged);
    CHECK((m.x == 10 && m.y == 9));
    CHECK((m.offsetX == 0 && m.offsetY == 0));
    CHECK(m.steps.empty());

    // A turn completes at once and the next step starts in the same frame.
    m.steps.push_back({10, 9, 0, 2, false});
    m.steps.push_back({11, 9, 0, 2, false});
    u = advanceMotion(m, nullptr, false, 1500, 0);
    CHECK(u.stepsCompleted == 1);
    CHECK(m.direction == 2);
    CHECK(m.steps.size() == 1);
}

TEST_CASE("movement: input intent from mouse and arrows")
{
    MovementInput in;
    MovementInputOptions opts;

    CHECK_FALSE(in.intent(400, 300, 400, 300, false, opts).has_value());

    in.setArrow(ArrowKey::Up, true);
    auto i = in.intent(400, 300, 0, 0, false, opts);
    REQUIRE(i.has_value());
    CHECK(i->direction == Direction::Up);
    CHECK_FALSE(i->fromMouse);
    CHECK_FALSE(in.intent(400, 300, 0, 0, true, opts).has_value());  // auto-walking

    // Right mouse held: walk toward the cursor, running when it is far away.
    in.rightMouseDown();
    i = in.intent(400, 300, 400, 250, true, opts);
    REQUIRE(i.has_value());
    CHECK(i->fromMouse);
    CHECK(i->cancelAutoWalk);
    CHECK_FALSE(i->run);
    i = in.intent(400, 300, 400, 50, false, opts);
    CHECK(i->run);

    // Left click while right-walking locks the walk on after release.
    in.leftMouseDown(opts);
    in.rightMouseUp();
    CHECK(in.mouseWalking());
    in.rightMouseDown();
    in.rightMouseUp();
    CHECK_FALSE(in.mouseWalking());
}

TEST_CASE("movement: A* gives up on an unreachable goal within the node budget")
{
    FakeTiles tiles;
    // Box in (50, 50) with a ring of walls; the goal tile itself is open, so no retargeting.
    for (int i = 47; i <= 53; i++)
    {
        tiles.wall(i, 47);
        tiles.wall(i, 53);
        tiles.wall(47, i);
        tiles.wall(53, i);
    }

    Pathfinder pf(tiles);
    CHECK_FALSE(pf.findPath(10, 10, 0, 50, 50, 0, 0));
    CHECK(pf.path().empty());
}
