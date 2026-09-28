// SPDX-License-Identifier: BSD-2-Clause
// Ported from ClassicUO (Game/Pathfinder.cs, Game/GameObjects/Land.cs).

#include "uo/movement/Pathfinder.h"

#include <algorithm>
#include <cstdlib>

namespace uo::movement
{

namespace
{

bool isDrawableLand(uint16_t graphic)
{
    // 0x01AE-0x01B5 and 0x01DB are the "no draw" void tiles; 2 is the black void.
    return (graphic < 0x01AE && graphic != 2) || (graphic > 0x01B5 && graphic != 0x01DB);
}

int chebyshev(int ax, int ay, int bx, int by)
{
    return std::max(std::abs(ax - bx), std::abs(ay - by));
}

}  // namespace

int stretchedLandAverageZ(const TileObject& land, int direction)
{
    // Corner order is {top, right, bottom, left}, as Land.GetDirectionZ indexes them.
    const int result = land.landCornerZ[((direction >> 1) + 1) & 3];
    if ((direction & 1) != 0)
    {
        return result;
    }
    return (result + land.landCornerZ[(direction >> 1) & 3]) >> 1;
}

void buildPathObjects(const std::vector<TileObject>& objects, const WalkerContext& ctx, std::vector<PathObject>& out)
{
    out.clear();

    const StepState state = ctx.stepState;

    for (const TileObject& obj : objects)
    {
        if (ctx.customHouseEditing && obj.z < ctx.playerZ)
        {
            continue;
        }

        const uint16_t graphic = obj.graphic;

        switch (obj.kind)
        {
        case TileObjectKind::Effect: break;

        case TileObjectKind::Land:
        {
            if (!isDrawableLand(graphic))
            {
                break;
            }

            uint32_t flags = PathObject::ImpassableOrSurface;

            if (state == StepState::OnSeaHorse)
            {
                if (obj.has(tile_flag::Wet))
                {
                    flags = PathObject::ImpassableOrSurface | PathObject::Surface | PathObject::Bridge;
                }
            }
            else
            {
                if (!obj.has(tile_flag::Impassable))
                {
                    flags = PathObject::ImpassableOrSurface | PathObject::Surface | PathObject::Bridge;
                }
                if (state == StepState::Flying && obj.has(tile_flag::NoDiagonal))
                {
                    flags |= PathObject::NoDiagonal;
                }
            }

            PathObject po;
            po.flags    = flags;
            po.z        = obj.landMinZ;
            po.averageZ = obj.landAverageZ;
            po.height   = obj.landAverageZ - obj.landMinZ;
            po.land     = obj.landStretched ? &obj : nullptr;
            out.push_back(po);
            break;
        }

        case TileObjectKind::Mobile:
        {
            if (!ctx.ignoreCharacters && !obj.mobileDead && !obj.mobileIgnoresCharacters)
            {
                PathObject po;
                po.flags    = PathObject::ImpassableOrSurface;
                po.z        = obj.z;
                po.averageZ = obj.z + kDefaultCharacterHeight;
                po.height   = kDefaultCharacterHeight;
                out.push_back(po);
            }
            break;
        }

        case TileObjectKind::Static:
        case TileObjectKind::Item:
        case TileObjectKind::Multi:
        {
            bool dropFlags = false;

            if (obj.kind == TileObjectKind::Item && !obj.itemIsMulti && !obj.has(tile_flag::Internal))
            {
                const bool isDoor = obj.has(tile_flag::Door);

                if (state == StepState::DeadOrGM && (isDoor || obj.itemWeight <= 0x5A || (ctx.isGameMaster && !obj.itemLocked)))
                {
                    dropFlags = true;
                }
                else if (ctx.smoothDoors && isDoor)
                {
                    dropFlags = true;
                }
                else
                {
                    dropFlags = (graphic >= 0x3946 && graphic <= 0x3964) || graphic == 0x0082;
                }
            }
            else if (obj.kind == TileObjectKind::Multi)
            {
                if ((ctx.customHouseEditing && obj.multiIsCustom && !obj.multiGenericInternal) || obj.multiHousePreview)
                {
                    break;
                }
                if (obj.multiIgnoreInRender)
                {
                    dropFlags = true;
                }
            }

            uint32_t flags = 0;

            if (state == StepState::OnSeaHorse)
            {
                if (obj.has(tile_flag::Wet))
                {
                    flags = PathObject::Surface | PathObject::Bridge;
                }
            }
            else
            {
                const bool impassable = obj.has(tile_flag::Impassable);

                if (impassable || obj.has(tile_flag::Surface))
                {
                    flags = PathObject::ImpassableOrSurface;
                }
                if (!impassable)
                {
                    if (obj.has(tile_flag::Surface))
                    {
                        flags |= PathObject::Surface;
                    }
                    if (obj.has(tile_flag::Bridge))
                    {
                        flags |= PathObject::Bridge;
                    }
                }

                if (state == StepState::DeadOrGM)
                {
                    if (graphic <= 0x0846)
                    {
                        if (graphic == 0x0846 || graphic == 0x0692 || (graphic > 0x06F4 && graphic <= 0x06F6))
                        {
                            dropFlags = true;
                        }
                    }
                    else if (graphic == 0x0873)
                    {
                        dropFlags = true;
                    }
                }

                if (dropFlags)
                {
                    flags &= ~static_cast<uint32_t>(PathObject::ImpassableOrSurface);
                }

                if (state == StepState::Flying && obj.has(tile_flag::NoDiagonal))
                {
                    flags |= PathObject::NoDiagonal;
                }
            }

            if (flags != 0)
            {
                int averageZ = obj.height;
                if (obj.has(tile_flag::Bridge))
                {
                    // ClassicUO keeps the truncating halving; rounding up breaks down-to-up stairs.
                    averageZ /= 2;
                }

                PathObject po;
                po.flags    = flags;
                po.z        = obj.z;
                po.averageZ = averageZ + obj.z;
                po.height   = obj.height;
                out.push_back(po);
            }
            break;
        }
        }
    }
}

Pathfinder::Pathfinder(ITileSource& tiles) : _tiles(tiles) {}

bool Pathfinder::collect(int x, int y, std::vector<TileObject>& tiles, std::vector<PathObject>& out)
{
    tiles.clear();
    out.clear();

    if (!_tiles.gatherTile(x, y, tiles))
    {
        return false;
    }

    // Stretched land in `out` points into `tiles`, which stays untouched until the next collect.
    buildPathObjects(tiles, _ctx, out);
    return !out.empty();
}

int Pathfinder::calculateMinMaxZ(int& minZ, int& maxZ, int newX, int newY, int currentZ, int newDirection)
{
    minZ = -128;
    maxZ = currentZ;
    newDirection &= 7;

    // Look at the tile being left, which is one step back along the direction of travel.
    offsetXY(toDirection(newDirection ^ 4), newX, newY);

    if (!collect(newX, newY, _minMaxTiles, _minMaxScratch))
    {
        return 0;
    }

    for (const PathObject& obj : _minMaxScratch)
    {
        const int averageZ = obj.averageZ;

        if (averageZ <= currentZ && obj.land != nullptr)
        {
            const int avgZ = stretchedLandAverageZ(*obj.land, newDirection);
            minZ = std::max(minZ, avgZ);
            maxZ = std::max(maxZ, avgZ);
        }
        else
        {
            if ((obj.flags & PathObject::ImpassableOrSurface) != 0 && averageZ <= currentZ && minZ < averageZ)
            {
                minZ = averageZ;
            }

            if ((obj.flags & PathObject::Bridge) != 0 && currentZ == averageZ)
            {
                maxZ = std::max(maxZ, obj.z + obj.height);
                minZ = std::min(minZ, obj.z);
            }
        }
    }

    maxZ += 2;
    return maxZ;
}

bool Pathfinder::calculateNewZ(int x, int y, int8_t& z, int direction)
{
    int minZ = -128;
    int maxZ = z;

    calculateMinMaxZ(minZ, maxZ, x, y, z, direction);

    if (_ctx.customHouseEditing)
    {
        // ClassicUO builds this rectangle with EndPos as width/height, which leaves the far edges
        // unchecked; the foundation spans [StartPos, EndPos) in world coordinates.
        if (x < _ctx.customHouseStartX || y < _ctx.customHouseStartY || x >= _ctx.customHouseEndX ||
            y >= _ctx.customHouseEndY)
        {
            return false;
        }
    }

    std::vector<PathObject>& list = _newZScratch;

    if (!collect(x, y, _newZTiles, list))
    {
        return false;
    }

    std::stable_sort(list.begin(), list.end(), [](const PathObject& a, const PathObject& b) {
        return a.z != b.z ? a.z < b.z : a.height < b.height;
    });

    PathObject sentinel;
    sentinel.flags    = PathObject::ImpassableOrSurface;
    sentinel.z        = 128;
    sentinel.averageZ = 128;
    sentinel.height   = 128;
    list.push_back(sentinel);

    int resultZ = -128;

    if (z < minZ)
    {
        z = static_cast<int8_t>(minZ);
    }

    int currentTempObjZ = 1000000;
    int currentZ = -128;

    for (size_t i = 0; i < list.size(); i++)
    {
        const PathObject& obj = list[i];

        if ((obj.flags & PathObject::NoDiagonal) != 0 && _ctx.stepState == StepState::Flying)
        {
            const int objAverageZ = obj.averageZ;

            if (std::abs(objAverageZ - z) <= 25)
            {
                resultZ = objAverageZ != -128 ? objAverageZ : currentZ;
                break;
            }
        }

        if ((obj.flags & PathObject::ImpassableOrSurface) != 0)
        {
            const int objZ = obj.z;

            if (objZ - minZ >= kDefaultBlockHeight)
            {
                for (size_t j = i; j-- > 0;)
                {
                    const PathObject& temp = list[j];

                    if ((temp.flags & (PathObject::Surface | PathObject::Bridge)) == 0)
                    {
                        continue;
                    }

                    const int tempAverageZ = temp.averageZ;

                    if (tempAverageZ >= currentZ && objZ - tempAverageZ >= kDefaultBlockHeight &&
                        ((tempAverageZ <= maxZ && (temp.flags & PathObject::Surface) != 0) ||
                         ((temp.flags & PathObject::Bridge) != 0 && temp.z <= maxZ)))
                    {
                        const int delta = std::abs(z - tempAverageZ);

                        if (delta < currentTempObjZ)
                        {
                            currentTempObjZ = delta;
                            resultZ = tempAverageZ;
                        }
                    }
                }
            }

            const int averageZ = obj.averageZ;
            minZ = std::max(minZ, averageZ);
            currentZ = std::max(currentZ, averageZ);
        }
    }

    z = static_cast<int8_t>(resultZ);
    return resultZ != -128;
}

bool Pathfinder::canWalk(Direction& direction, int& x, int& y, int8_t& z)
{
    int newX = x;
    int newY = y;
    int8_t newZ = z;
    int newDirection = toByte(direction);

    offsetXY(direction, newX, newY);
    bool passed = calculateNewZ(newX, newY, newZ, newDirection);

    if (isDiagonal(direction))
    {
        constexpr int dirOffset[2] = {1, -1};

        // A diagonal needs both flanking cardinals open.
        for (int i = 0; i < 2 && passed; i++)
        {
            int testX = x;
            int testY = y;
            int8_t testZ = z;
            const int testDir = (toByte(direction) + dirOffset[i]) % 8;
            offsetXY(toDirection(testDir), testX, testY);
            passed = calculateNewZ(testX, testY, testZ, testDir);
        }

        // Otherwise slide to whichever flanking cardinal is open.
        if (!passed)
        {
            for (int i = 0; i < 2 && !passed; i++)
            {
                newX = x;
                newY = y;
                newZ = z;
                newDirection = (toByte(direction) + dirOffset[i]) % 8;
                offsetXY(toDirection(newDirection), newX, newY);
                passed = calculateNewZ(newX, newY, newZ, newDirection);
            }
        }
    }

    if (passed)
    {
        x = newX;
        y = newY;
        z = newZ;
        direction = toDirection(newDirection);
    }

    return passed;
}

bool Pathfinder::isBlocked(int x, int y, int z)
{
    int8_t tempZ = static_cast<int8_t>(z);
    return !calculateNewZ(x, y, tempZ, toByte(Direction::North));
}

int Pathfinder::goalDistCost(int x, int y) const
{
    return chebyshev(_endX, _endY, x, y);
}

int Pathfinder::addOpen(int direction, int x, int y, int z, int parentClosed, int cost)
{
    const uint64_t k = key(x, y, z);

    if (_closedIndex.contains(k))
    {
        return 0;
    }

    const Node& parent = _closed[parentClosed];

    if (auto it = _openIndex.find(k); it != _openIndex.end())
    {
        Node& node = _open[it->second];
        const int startCost = parent.distFromStart + cost;

        if (node.distFromStart > startCost)
        {
            node.parent = parentClosed;
            // ClassicUO adds the step cost twice here; kept so paths match the C# client.
            node.distFromStart = startCost + cost;
            node.cost = node.distFromGoal + node.distFromStart;
        }
        return it->second;
    }

    for (int i = 0; i < kPathfinderMaxNodes; i++)
    {
        Node& node = _open[i];

        if (node.used)
        {
            continue;
        }

        node.used          = true;
        node.direction     = direction;
        node.x             = x;
        node.y             = y;
        node.z             = z;
        node.distFromGoal  = goalDistCost(x, y);
        node.distFromStart = parent.distFromStart + cost;
        node.cost          = node.distFromGoal + node.distFromStart;
        node.parent        = parentClosed;

        _openIndex[k] = i;
        _openHighWater = std::max(_openHighWater, i + 1);
        _openCount++;

        if (chebyshev(_endX, _endY, x, y) <= _pathfindDistance)
        {
            _goalNode = i;
        }
        return i;
    }

    return -1;
}

int Pathfinder::closeCheapest()
{
    int cheapestCost = 9999999;
    int cheapest = -1;

    for (int i = 0; i < _openHighWater; i++)
    {
        if (_open[i].used && _open[i].cost < cheapestCost)
        {
            cheapest = i;
            cheapestCost = _open[i].cost;
        }
    }

    if (cheapest == -1)
    {
        return -1;
    }

    Node& from = _open[cheapest];
    from.used = false;
    _openIndex.erase(key(from.x, from.y, from.z));
    _openCount--;

    if (_closed.size() >= static_cast<size_t>(kPathfinderMaxNodes))
    {
        return -1;
    }

    Node closed = from;
    closed.used = true;
    _closed.push_back(closed);

    const int index = static_cast<int>(_closed.size()) - 1;
    _closedIndex[key(closed.x, closed.y, closed.z)] = index;
    return index;
}

bool Pathfinder::openNodes(int closedIndex)
{
    bool found = false;
    const Node from = _closed[closedIndex];

    for (int i = 0; i < 8; i++)
    {
        Direction direction = toDirection(i);
        int x = from.x;
        int y = from.y;
        int8_t z = static_cast<int8_t>(from.z);

        if (!canWalk(direction, x, y, z) || toByte(direction) != i)
        {
            continue;
        }

        const int cost = (i % 2) == 0 ? 1 : 2;

        if (addOpen(i, x, y, z, closedIndex, cost) != -1)
        {
            found = true;
        }
    }

    return found;
}

bool Pathfinder::findPath(int startX, int startY, int8_t startZ, int x, int y, int z, int distance)
{
    _path.clear();
    _run = false;

    // A goal that cannot be stood on (a tree, a chest) is reached from a neighbouring tile instead
    // of searching the whole budget for it.
    if (distance == 0 && isBlocked(x, y, z))
    {
        distance = 1;
    }

    _open.assign(kPathfinderMaxNodes, Node{});
    _closed.clear();
    _closed.reserve(1024);
    _openIndex.clear();
    _closedIndex.clear();
    _openHighWater = 0;
    _openCount = 0;
    _endX = x;
    _endY = y;
    _pathfindDistance = distance;
    _goalNode = -1;

    Node start;
    start.used = true;
    start.x = startX;
    start.y = startY;
    start.z = startZ;
    start.distFromGoal = goalDistCost(startX, startY);
    start.cost = start.distFromGoal;
    _closed.push_back(start);
    _closedIndex[key(startX, startY, startZ)] = 0;

    if (start.distFromGoal > 14)
    {
        _run = true;
    }

    int current = 0;

    while (true)
    {
        openNodes(current);

        if (_goalNode != -1)
        {
            // Walk back from the goal through closed parents, then reverse.
            std::vector<PathStep> reversed;
            const Node& goal = _open[_goalNode];
            reversed.push_back({goal.x, goal.y, static_cast<int8_t>(goal.z), toDirection(goal.direction)});

            for (int p = goal.parent; p != -1; p = _closed[p].parent)
            {
                const Node& n = _closed[p];
                reversed.push_back({n.x, n.y, static_cast<int8_t>(n.z), toDirection(n.direction)});
            }

            _path.assign(reversed.rbegin(), reversed.rend());
            return true;
        }

        current = closeCheapest();

        if (current == -1)
        {
            _run = false;
            return false;
        }
    }
}

}  // namespace uo::movement
