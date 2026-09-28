// SPDX-License-Identifier: BSD-2-Clause
// Ported from ClassicUO (Pathfinder.WalkTo / ProcessAutoWalk).

#pragma once

#include "uo/movement/Pathfinder.h"

#include <optional>

namespace uo::movement
{

// Drives the player along a planned path one walk request at a time.
class AutoWalker
{
public:
    struct Request
    {
        Direction direction;
        bool run;
    };

    // Plans a path from the player's tile to within `distance` of (x, y, z). Returns false, and
    // stays idle, when there is none.
    bool start(Pathfinder& pathfinder, int fromX, int fromY, int8_t fromZ, int x, int y, int z, int distance);

    // Next request to feed Walker::walk, or nothing this frame. `endDirection` is the direction the
    // player will face once queued steps finish. When Walker::walk rejects the request, call stop().
    std::optional<Request> tick(Direction endDirection, bool walkerCanRequest);

    void stop();

    bool active() const { return _active; }
    bool cancellable() const { return _cancellable; }
    void setCancellable(bool value) { _cancellable = value; }

private:
    std::vector<PathStep> _path;
    size_t _pointIndex{0};
    bool _run{false};
    bool _active{false};
    bool _cancellable{true};
};

}  // namespace uo::movement
