// SPDX-License-Identifier: BSD-2-Clause
// Ported from ClassicUO (Game/Pathfinder.cs): walkability, z resolution, and A* auto-walk.

#pragma once

#include "uo/movement/Direction.h"
#include "uo/movement/MovementConstants.h"
#include "uo/movement/TileQuery.h"

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace uo::movement
{

// Collision object derived from a TileObject for one step state.
struct PathObject
{
    enum Flags : uint32_t
    {
        ImpassableOrSurface = 0x1,
        Surface             = 0x2,
        Bridge              = 0x4,
        NoDiagonal          = 0x8,
    };

    uint32_t flags{0};
    int z{0};
    int averageZ{0};
    int height{0};
    const TileObject* land{nullptr};  // set for stretched land, which resolves z per direction
};

// Converts a tile's objects into collision objects (Pathfinder.CreateItemList). Exposed for tests.
void buildPathObjects(const std::vector<TileObject>& objects, const WalkerContext& ctx, std::vector<PathObject>& out);

// Z of stretched land approached from `direction` (Land.CalculateCurrentAverageZ).
int stretchedLandAverageZ(const TileObject& land, int direction);

struct PathStep
{
    int x{0}, y{0};
    int8_t z{0};
    Direction direction{Direction::North};
};

class Pathfinder
{
public:
    explicit Pathfinder(ITileSource& tiles);

    void setContext(const WalkerContext& ctx) { _ctx = ctx; }
    const WalkerContext& context() const { return _ctx; }

    // Resolves the z the player would stand at on (x, y) arriving from `direction`, starting at `z`.
    // Returns false when nothing there can be stood on.
    bool calculateNewZ(int x, int y, int8_t& z, int direction);

    // One step from (x, y, z) in `direction`. Diagonal steps must clear both adjacent cardinals;
    // a blocked diagonal slides to whichever neighbouring direction is open. On success the in/out
    // values hold the new tile and the direction actually taken.
    bool canWalk(Direction& direction, int& x, int& y, int8_t& z);

    // Plans a path from (startX, startY, startZ) to within `distance` tiles of (x, y). Replaces any
    // current plan. Returns false when no path exists within the node budget.
    bool findPath(int startX, int startY, int8_t startZ, int x, int y, int z, int distance);

    // The planned path, start tile first; empty when there is none.
    const std::vector<PathStep>& path() const { return _path; }

    // True when a search started past 14 tiles, in which case the whole auto-walk runs.
    bool wantsRun() const { return _run; }

    // Whether z at (x, y) is unreachable (used to retarget blocked goals to a neighbouring tile).
    bool isBlocked(int x, int y, int z);

private:
    struct Node
    {
        int x{0}, y{0}, z{0};
        int direction{0};
        int cost{0};
        int distFromStart{0};
        int distFromGoal{0};
        int parent{-1};  // index into _closed
        bool used{false};
    };

    int calculateMinMaxZ(int& minZ, int& maxZ, int newX, int newY, int currentZ, int newDirection);
    bool collect(int x, int y, std::vector<TileObject>& tiles, std::vector<PathObject>& out);

    int goalDistCost(int x, int y) const;
    int addOpen(int direction, int x, int y, int z, int parentClosed, int cost);
    int closeCheapest();
    bool openNodes(int closedIndex);

    static uint64_t key(int x, int y, int z)
    {
        return (static_cast<uint64_t>(static_cast<uint16_t>(x)) << 24) |
               (static_cast<uint64_t>(static_cast<uint16_t>(y)) << 8) | static_cast<uint8_t>(z);
    }

    ITileSource& _tiles;
    WalkerContext _ctx{};

    std::vector<TileObject> _minMaxTiles;
    std::vector<TileObject> _newZTiles;
    std::vector<PathObject> _minMaxScratch;
    std::vector<PathObject> _newZScratch;

    // A* state. Open slots are reused in index order and the cheapest is the lowest index among
    // equal costs, matching ClassicUO's tie-breaking so paths come out the same.
    std::vector<Node> _open;
    std::vector<Node> _closed;
    std::unordered_map<uint64_t, int> _openIndex;
    std::unordered_map<uint64_t, int> _closedIndex;
    int _openHighWater{0};
    int _openCount{0};
    int _endX{0}, _endY{0};
    int _pathfindDistance{0};
    int _goalNode{-1};
    bool _run{false};
    std::vector<PathStep> _path;
};

}  // namespace uo::movement
