// SPDX-License-Identifier: BSD-2-Clause
// Ported from ClassicUO (Network/PacketHandlers.cs DenyWalk, ConfirmWalk, MovePlayer, Pathfinding,
// Swing, ExtendedCommand 1/2; Game/Map tile lists and Land.ApplyStretch; GameScene.Update).

#include "uo/movement/MovementSystem.h"

#include "uo/assets/Map.h"
#include "uo/assets/TileData.h"
#include "uo/io/BinaryReader.h"
#include "uo/movement/MovementPackets.h"
#include "uo/net/OutgoingPackets.h"
#include "uo/world/PacketHandlers.h"
#include "uo/world/World.h"

#include <algorithm>
#include <chrono>

namespace uo::movement
{

namespace
{

// uo::movement::Direction is uo::world::Direction; these keep the call sites readable.
Direction toMove(world::Direction d) { return d; }
world::Direction toWorld(Direction d) { return d; }

bool flatNormal(int tile, int top, int right, int bottom, int left)
{
    return tile == top && tile == right && tile == bottom && tile == left;
}

uint64_t steadyMs()
{
    using namespace std::chrono;
    return static_cast<uint64_t>(duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count());
}

constexpr int kTurnToLastTargetMs = 2000;

}  // namespace

// --- WorldTileSource -------------------------------------------------------------------------

void WorldTileSource::setMap(const assets::MapFacet* map)
{
    _map = map;
    _staticBlocks.clear();
}

int8_t WorldTileSource::landZ(int x, int y) const
{
    if (_map == nullptr || x < 0 || y < 0 || x >= _map->width() || y >= _map->height())
    {
        return -125;  // ClassicUO's GetTileZ for out-of-map cells
    }
    return _map->land(x, y).z;
}

void WorldTileSource::appendLand(int x, int y, std::vector<TileObject>& out) const
{
    const assets::LandCell cell = _map->land(x, y);
    const assets::LandTile* data = _tiledata.landTile(cell.tileId);

    TileObject land;
    land.kind = TileObjectKind::Land;
    land.graphic = cell.tileId;
    land.z = cell.z;
    land.tileFlags = data ? data->flags : 0;

    const uint16_t texId = data ? data->texId : 0;
    const bool wetVoid = texId == 0 && land.has(tile_flag::Wet);
    const bool textured = hasTexmap ? hasTexmap(texId) : texId != 0;

    // Land.ApplyStretch: untextured land (and wet texture-less water) is drawn flat.
    if (wetVoid || !textured)
    {
        land.landMinZ = land.landAverageZ = cell.z;
        land.landCornerZ = {cell.z, cell.z, cell.z, cell.z};
        out.push_back(land);
        return;
    }

    const int8_t zTop = cell.z;
    const int8_t zRight = landZ(x + 1, y);
    const int8_t zLeft = landZ(x, y + 1);
    const int8_t zBottom = landZ(x + 1, y + 1);

    land.landCornerZ = {zTop, zRight, zBottom, zLeft};
    land.landAverageZ = std::abs(zTop - zBottom) <= std::abs(zLeft - zRight)
                            ? static_cast<int8_t>((zTop + zBottom) >> 1)
                            : static_cast<int8_t>((zLeft + zRight) >> 1);
    land.landMinZ = std::min({zTop, zRight, zLeft, zBottom});

    const int t10 = landZ(x, y - 1), t20 = landZ(x + 1, y - 1), t01 = landZ(x - 1, y), t31 = landZ(x + 2, y);
    const int t02 = landZ(x - 1, y + 1), t32 = landZ(x + 2, y + 1), t13 = landZ(x, y + 2), t23 = landZ(x + 1, y + 2);

    land.landStretched = !flatNormal(zTop, t10, zRight, zLeft, t01) || !flatNormal(zRight, t20, t31, zBottom, zTop) ||
                         !flatNormal(zBottom, zRight, t32, t23, zLeft) || !flatNormal(zLeft, zTop, zBottom, t13, t02);
    out.push_back(land);
}

void WorldTileSource::appendStatics(int x, int y, std::vector<TileObject>& out)
{
    const int bx = x / assets::MapFacet::kBlockSize;
    const int by = y / assets::MapFacet::kBlockSize;
    const uint64_t key = tileKey(bx, by);

    auto it = _staticBlocks.find(key);
    if (it == _staticBlocks.end())
    {
        std::vector<StaticEntry> block;
        for (const assets::StaticCell& cell : _map->staticsBlock(bx, by))
        {
            const assets::StaticTile* data = _tiledata.staticTile(cell.graphic);
            StaticEntry e;
            e.cell = static_cast<uint8_t>(cell.y * assets::MapFacet::kBlockSize + cell.x);
            e.object.kind = TileObjectKind::Static;
            e.object.graphic = cell.graphic;
            e.object.z = cell.z;
            e.object.tileFlags = data ? data->flags : 0;
            e.object.height = data ? data->height : 0;
            block.push_back(e);
        }
        it = _staticBlocks.emplace(key, std::move(block)).first;
    }

    const auto cell = static_cast<uint8_t>((y % assets::MapFacet::kBlockSize) * assets::MapFacet::kBlockSize +
                                           x % assets::MapFacet::kBlockSize);
    for (const StaticEntry& e : it->second)
    {
        if (e.cell == cell)
        {
            out.push_back(e.object);
        }
    }
}

void WorldTileSource::rebuild()
{
    for (auto& [key, list] : _dynamic)
    {
        list.clear();
    }

    const world::Player* player = _world.player();

    _world.forEachItem([&](world::Item& item) {
        // Multi components come from the multi loader, which is not ported yet.
        if (!item.onGround() || item.isMulti)
        {
            return;
        }

        const assets::StaticTile* data = _tiledata.staticTile(item.graphic);
        TileObject o;
        o.kind = TileObjectKind::Item;
        o.graphic = item.graphic;
        o.z = item.z;
        o.tileFlags = data ? data->flags : 0;
        o.height = data ? data->height : 0;
        o.itemWeight = data ? data->weight : 0;
        // Item.IsLocked: not movable and heavier than a lift can take.
        o.itemLocked = (item.flagBits & world::flags::Movable) == 0 && o.itemWeight > 90;
        _dynamic[tileKey(item.x, item.y)].push_back(o);
    });

    _world.forEachMobile([&](world::Mobile& m) {
        if (&m == player)
        {
            return;
        }

        TileObject o;
        o.kind = TileObjectKind::Mobile;
        o.graphic = m.graphic;
        o.z = m.z;
        o.mobileDead = m.isDead();
        o.mobileIgnoresCharacters = m.ignoreMobiles();
        _dynamic[tileKey(m.x, m.y)].push_back(o);
    });
}

bool WorldTileSource::gatherTile(int x, int y, std::vector<TileObject>& out)
{
    if (_map == nullptr || x < 0 || y < 0 || x >= _map->width() || y >= _map->height())
    {
        return false;
    }

    appendLand(x, y, out);
    appendStatics(x, y, out);

    if (auto it = _dynamic.find(tileKey(x, y)); it != _dynamic.end())
    {
        out.insert(out.end(), it->second.begin(), it->second.end());
    }
    return true;
}

// --- MovementSystem --------------------------------------------------------------------------

MovementSystem::MovementSystem(world::World& world, ITileSource& tiles, Send send)
    : now(steadyMs), _world(world), _send(std::move(send)), _pathfinder(tiles)
{
}

void MovementSystem::install(world::PacketHandlers& handlers)
{
    handlers.setHook(packets::kDenyWalk, [this](world::World&, io::BinaryReader& r) { denyWalk(r); });
    handlers.setHook(packets::kConfirmWalk, [this](world::World&, io::BinaryReader& r) { confirmWalk(r); });
    handlers.setHook(packets::kMovePlayer, [this](world::World&, io::BinaryReader& r) { movePlayer(r); });
    handlers.setHook(0x38, [this](world::World&, io::BinaryReader& r) { serverPathfind(r); });
    handlers.setHook(0x2F, [this](world::World&, io::BinaryReader& r) { swing(r); });

    handlers.setExtendedHook(packets::kExtFastWalkKeys, [this](world::World&, io::BinaryReader& r) {
        for (int i = 0; i < 6; i++)
        {
            _walker.fastWalk().set(i, r.readU32BE());
        }
    });
    handlers.setExtendedHook(packets::kExtFastWalkAddKey,
                             [this](world::World&, io::BinaryReader& r) { _walker.fastWalk().add(r.readU32BE()); });
}

bool MovementSystem::mounted(const world::Mobile& m) const
{
    return _world.findItemByLayer(m, world::Layer::Mount) != nullptr ||
           m.speedMode == world::CharacterSpeed::FastUnmount ||
           m.speedMode == world::CharacterSpeed::FastUnmountAndCantRun || m.isFlying(_world.clientVersion);
}

void MovementSystem::refreshContext()
{
    const world::Player* p = _world.player();
    if (p == nullptr)
    {
        return;
    }

    const world::Item* mount = _world.findItemByLayer(*p, world::Layer::Mount);
    const bool gargoyleFlying = p->race == world::Race::Gargoyle && p->isFlying(_world.clientVersion);

    WalkerContext ctx;
    ctx.stepState = stepStateFor(p->isDead(), p->graphic, gargoyleFlying, mount ? mount->graphic : 0);
    ctx.isGameMaster = p->graphic == kGameMasterBody;
    ctx.playerZ = p->z;
    ctx.ignoreCharacters = WalkerContext::ignoresCharacters(options.ignoreStaminaCheck, ctx.stepState,
                                                            p->ignoreMobiles(), p->stamina, p->staminaMax,
                                                            _world.mapIndex);
    ctx.smoothDoors = options.smoothDoors;
    _pathfinder.setContext(ctx);
}

PlayerWalkState MovementSystem::playerState() const
{
    const world::Player& p = *_world.player();

    PlayerWalkState s;
    world::Direction endDir;
    p.endPosition(s.x, s.y, s.z, endDir);
    s.facing = toMove(p.direction);
    s.hasQueuedSteps = !p.steps.empty();
    s.lastQueuedDirection = toMove(endDir);
    s.paralyzed = p.isParalyzed();
    s.dead = p.isDead();
    s.hidden = p.isHidden();
    s.mounted = mounted(p);
    s.speed = static_cast<CharacterSpeed>(p.speedMode);
    s.stamina = p.stamina;
    return s;
}

WalkResult MovementSystem::walk(Direction direction, bool run)
{
    world::Player* p = _world.player();
    if (p == nullptr || !_world.inGame())
    {
        return {};
    }

    refreshContext();
    options.walk.paralysisBlocksWalk = _world.clientVersion >= makeVersion(6, 0, 14, 2);

    const PlayerWalkState state = playerState();
    const uint64_t t = now();
    WalkResult r = _walker.walk(direction, run, state, options.walk, _pathfinder, t);

    switch (r.kind)
    {
    case WalkResult::Kind::Rejected: break;

    case WalkResult::Kind::Coalesced: p->direction = toWorld(r.direction); break;

    case WalkResult::Kind::Sent:
        if (r.startsFromIdle)
        {
            _clocks[p->serial].lastStepTime = t;
        }
        p->steps.push_back({r.x, r.y, r.z, toWorld(r.direction), r.run});
        if (_send)
        {
            _send(r.packet);
        }
        break;
    }

    return r;
}

bool MovementSystem::walkTo(int x, int y, int z, int distance)
{
    const world::Player* p = _world.player();
    if (p == nullptr || p->isParalyzed())
    {
        return false;
    }

    refreshContext();
    return _auto.start(_pathfinder, p->x, p->y, p->z, x, y, z, distance);
}

void MovementSystem::update(const std::optional<MovementIntent>& intent, int frameDelayMs)
{
    world::Player* p = _world.player();
    const uint64_t t = now();

    if (p != nullptr && _world.inGame())
    {
        // GameScene.Update order: auto-walk, then mouse or arrow keys.
        int ex, ey;
        int8_t ez;
        world::Direction endDir;
        p->endPosition(ex, ey, ez, endDir);

        if (auto req = _auto.tick(toMove(endDir), _walker.canRequestStep(t)))
        {
            if (walk(req->direction, req->run).kind == WalkResult::Kind::Rejected)
            {
                _auto.stop();
            }
        }

        if (intent)
        {
            if (intent->cancelAutoWalk)
            {
                _auto.stop();
            }
            walk(intent->direction, intent->run);
        }
    }

    _world.forEachMobile([&](world::Mobile& m) {
        const bool isPlayer = &m == p;
        if (m.steps.empty())
        {
            if (auto it = _clocks.find(m.serial); it != _clocks.end())
            {
                it->second.lastStepTime = t;
            }
            return;
        }

        MotionClock& clock = _clocks[m.serial];
        if (clock.lastStepTime == 0)
        {
            clock.lastStepTime = t;
        }

        const MotionUpdate u = advanceMotion(m, clock, isPlayer ? &_walker : nullptr, mounted(m), t, frameDelayMs);
        if (u.fell && onFell)
        {
            onFell();
        }
        if (u.tileChanged)
        {
            _world.listener().onEntityUpdated(m);
        }
    });

    // Forget clocks of mobiles that left.
    std::erase_if(_clocks, [this](const auto& kv) { return _world.mobile(kv.first) == nullptr; });
}

const MotionClock* MovementSystem::motion(uint32_t serial) const
{
    auto it = _clocks.find(serial);
    return it == _clocks.end() ? nullptr : &it->second;
}

void MovementSystem::onPlayerTeleported()
{
    if (const world::Player* p = _world.player())
    {
        _walker.resetFromServer(toMove(p->direction));
        _clocks.erase(p->serial);
    }
}

void MovementSystem::denyWalk(io::BinaryReader& r)
{
    world::Player* p = _world.player();
    if (p == nullptr)
    {
        return;
    }

    r.readU8();  // sequence
    const uint16_t x = r.readU16BE();
    const uint16_t y = r.readU16BE();
    const Direction dir = masked(toDirection(r.readU8()));
    const int8_t z = r.readI8();

    p->clearSteps();
    _clocks.erase(p->serial);
    _walker.deny(dir);

    _world.rangeX = x;
    _world.rangeY = y;
    p->x = x;
    p->y = y;
    p->z = z;
    p->direction = toWorld(dir);
    _world.weather.reset();
    _world.listener().onEntityUpdated(*p);
}

void MovementSystem::confirmWalk(io::BinaryReader& r)
{
    world::Player* p = _world.player();
    if (p == nullptr)
    {
        return;
    }

    const uint8_t sequence = r.readU8();
    uint8_t noto = r.readU8() & ~0x40;
    if (noto == 0 || noto >= 8)
    {
        noto = 1;
    }
    p->notoriety = static_cast<world::Notoriety>(noto);

    const ConfirmResult c = _walker.confirm(sequence);
    if (c.sendResync && _send)
    {
        const auto packet = net::out::resync();
        _send(packet);
    }
    if (c.rangeCenter)
    {
        _world.rangeX = c.rangeCenter->first;
        _world.rangeY = c.rangeCenter->second;
    }
    _world.listener().onEntityUpdated(*p);
}

void MovementSystem::movePlayer(io::BinaryReader& r)
{
    if (!_world.inGame())
    {
        return;
    }
    const Direction dir = toDirection(r.readU8());
    walk(masked(dir), isRunning(dir));
}

void MovementSystem::serverPathfind(io::BinaryReader& r)
{
    if (!_world.inGame())
    {
        return;
    }
    const uint16_t x = r.readU16BE();
    const uint16_t y = r.readU16BE();
    const uint16_t z = r.readU16BE();
    walkTo(x, y, static_cast<int16_t>(z), 0);
}

void MovementSystem::swing(io::BinaryReader& r)
{
    world::Player* p = _world.player();
    if (p == nullptr || !_world.inGame())
    {
        return;
    }

    r.skip(1);
    if (r.readU32BE() != p->serial)
    {
        return;
    }
    const uint32_t defender = r.readU32BE();

    // Turn to face the last attacked target when it swings at us while we stand still in war mode.
    if (_world.target.lastAttack != defender || !p->inWarMode() ||
        _walker.lastStepRequestTime() + kTurnToLastTargetMs >= now() || !p->steps.empty())
    {
        return;
    }

    const world::Mobile* enemy = _world.mobile(defender);
    if (enemy == nullptr)
    {
        return;
    }

    refreshContext();
    Direction dir = directionAB(p->x, p->y, enemy->x, enemy->y);
    int x = p->x;
    int y = p->y;
    int8_t z = p->z;

    if (_pathfinder.canWalk(dir, x, y, z) && toMove(p->direction) != dir)
    {
        walk(dir, false);
    }
}

}  // namespace uo::movement
