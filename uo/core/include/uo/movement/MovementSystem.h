// SPDX-License-Identifier: BSD-2-Clause
// Movement wired into uo::world: the player's walk requests and the server's answers, auto-walk,
// and the per-frame step advance of every mobile. Ported from the ClassicUO call sites that tie
// WalkerManager, Pathfinder and Mobile.ProcessSteps to the world (PacketHandlers DenyWalk,
// ConfirmWalk, MovePlayer, Pathfinding, Swing and ExtendedCommand 1/2; GameScene.Update).

#pragma once

#include "uo/io/BinaryReader.h"
#include "uo/movement/AutoWalker.h"
#include "uo/movement/MobileMotion.h"
#include "uo/movement/MovementInput.h"
#include "uo/movement/Pathfinder.h"
#include "uo/movement/Walker.h"

#include <cstdint>
#include <functional>
#include <span>
#include <unordered_map>
#include <vector>

namespace uo::assets
{
class MapFacet;
class TileData;
struct MultiComponent;
}  // namespace uo::assets

namespace uo::world
{
class Mobile;
class PacketHandlers;
class World;
}  // namespace uo::world

namespace uo::movement
{

// The tiles of the current facet plus what the world has placed on them: land (with the corner
// heights of stretched terrain), statics, ground items and mobiles.
class WorldTileSource : public ITileSource
{
public:
    WorldTileSource(world::World& world, const assets::TileData& tiledata) : _world(world), _tiledata(tiledata) {}

    void setMap(const assets::MapFacet* map);

    // Re-indexes ground items and mobiles by tile. Call once per frame before walking.
    void rebuild();

    bool gatherTile(int x, int y, std::vector<TileObject>& out) override;

    // Whether land texture `texId` exists in texmaps; stretched land needs one. Until the texmap
    // loader lands every non-zero id counts.
    std::function<bool(uint16_t texId)> hasTexmap;

    // Components of multi `id` (a house or boat item's graphic) from the multi loader, or null
    // when there is none. Without it multi items block nothing.
    std::function<const std::vector<assets::MultiComponent>*(uint16_t id)> multiComponents;

private:
    int8_t landZ(int x, int y) const;
    void appendLand(int x, int y, std::vector<TileObject>& out) const;
    void appendStatics(int x, int y, std::vector<TileObject>& out);

    static uint64_t tileKey(int x, int y)
    {
        return (static_cast<uint64_t>(static_cast<uint32_t>(x)) << 32) | static_cast<uint32_t>(y);
    }

    world::World& _world;
    const assets::TileData& _tiledata;
    const assets::MapFacet* _map{nullptr};
    std::unordered_map<uint64_t, std::vector<TileObject>> _dynamic;
    struct StaticEntry
    {
        uint8_t cell{0};  // y * 8 + x within the block
        TileObject object;
    };
    std::unordered_map<uint64_t, std::vector<StaticEntry>> _staticBlocks;  // statics by block, cached
};

struct MovementOptions
{
    WalkOptions walk;
    MovementInputOptions input;
    bool ignoreStaminaCheck{false};
    bool smoothDoors{false};
};

class MovementSystem
{
public:
    using Send = std::function<void(std::span<const uint8_t>)>;

    MovementSystem(world::World& world, ITileSource& tiles, Send send);

    // Routes 0x21, 0x22, 0x97, 0x38, 0x2F and 0xBF 0x01/0x02 to this system.
    void install(world::PacketHandlers& handlers);

    // WorldListener::onPlayerTeleported (0x20 and the player's own 0x78/0xF3): the server placed
    // the player, so every walk request in flight is void.
    void onPlayerTeleported();

    // Ask to turn or step. Queues the step on the player and sends 0x02 when the walker allows.
    WalkResult walk(Direction direction, bool run);

    // Pathfind to within `distance` of (x, y, z) and walk there over the next frames.
    bool walkTo(int x, int y, int z, int distance);
    void stopAutoWalk() { _auto.stop(); }

    // One frame: auto-walk, then the player's input intent, then every mobile's step advance.
    void update(const std::optional<MovementIntent>& intent, int frameDelayMs);

    // Draw offset of a mobile this frame (zero when it is standing on its tile).
    const MotionClock* motion(uint32_t serial) const;

    MovementOptions options;
    std::function<uint64_t()> now;  // milliseconds; the scene's clock

    Walker& walker() { return _walker; }
    AutoWalker& autoWalker() { return _auto; }
    Pathfinder& pathfinder() { return _pathfinder; }

    // Called when the player drops 22 z or more in one step (ClassicUO prints "Ouch!").
    std::function<void()> onFell;

private:
    void refreshContext();
    PlayerWalkState playerState() const;
    bool mounted(const world::Mobile& m) const;

    void denyWalk(io::BinaryReader& r);
    void confirmWalk(io::BinaryReader& r);
    void movePlayer(io::BinaryReader& r);
    void serverPathfind(io::BinaryReader& r);
    void swing(io::BinaryReader& r);

    world::World& _world;
    Send _send;
    Pathfinder _pathfinder;
    Walker _walker;
    AutoWalker _auto;
    std::unordered_map<uint32_t, MotionClock> _clocks;
};

}  // namespace uo::movement
