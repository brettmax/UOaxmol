// SPDX-License-Identifier: BSD-2-Clause
// Ported from ClassicUO (Pathfinder.WalkTo / ProcessAutoWalk).

#include "uo/movement/AutoWalker.h"

namespace uo::movement
{

bool AutoWalker::start(Pathfinder& pathfinder, int fromX, int fromY, int8_t fromZ, int x, int y, int z, int distance)
{
    stop();
    _cancellable = true;

    if (!pathfinder.findPath(fromX, fromY, fromZ, x, y, z, distance))
    {
        return false;
    }

    _path = pathfinder.path();
    _run = pathfinder.wantsRun();
    _pointIndex = 1;  // index 0 is the tile the player stands on
    _active = true;
    return !_path.empty();
}

std::optional<AutoWalker::Request> AutoWalker::tick(Direction endDirection, bool walkerCanRequest)
{
    if (!_active || !walkerCanRequest)
    {
        return std::nullopt;
    }

    if (_pointIndex >= _path.size())
    {
        stop();
        return std::nullopt;
    }

    const PathStep& p = _path[_pointIndex];

    // The first request toward a node turns; once facing it, the same request steps and the walk
    // moves on to the next node.
    if (masked(endDirection) == p.direction)
    {
        _pointIndex++;
    }

    return Request{p.direction, _run};
}

void AutoWalker::stop()
{
    _active = false;
    _run = false;
    _path.clear();
    _pointIndex = 0;
}

}  // namespace uo::movement
