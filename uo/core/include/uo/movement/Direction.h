// SPDX-License-Identifier: BSD-2-Clause
// Ported from ClassicUO (Game/Data/Direction.cs, Game/GameCursor.cs, Game/Pathfinder.cs).

#pragma once

#include "uo/world/Types.h"

#include <cstdint>

namespace uo::movement
{

// World directions as the protocol encodes them (uo::world::Direction). North is y-1, which
// draws up and to the right on screen; the names follow ClassicUO.
using Direction = uo::world::Direction;

// ClassicUO's Direction.NONE: no direction (no arrow held, or a zero delta).
inline constexpr Direction kDirectionNone = Direction::None;

constexpr uint8_t toByte(Direction d) { return static_cast<uint8_t>(d); }
constexpr Direction toDirection(int v) { return static_cast<Direction>(static_cast<uint8_t>(v)); }
constexpr Direction masked(Direction d) { return toDirection(toByte(d) & 7); }
constexpr bool isRunning(Direction d) { return (toByte(d) & 0x80) != 0; }
constexpr bool isDiagonal(Direction d) { return (toByte(d) & 1) != 0; }
constexpr Direction reverse(Direction d) { return toDirection((toByte(d) + 4) & 7); }
constexpr Direction cardinal(Direction d) { return toDirection(toByte(d) & 6); }

// Moves (x, y) one tile in the given direction (only the low three bits are used).
constexpr void offsetXY(Direction d, int& x, int& y)
{
    constexpr int dx[8] = {0, 1, 1, 1, 0, -1, -1, -1};
    constexpr int dy[8] = {-1, -1, 0, 1, 1, 1, 0, -1};
    x += dx[toByte(d) & 7];
    y += dy[toByte(d) & 7];
}

// Direction from tile A to tile B through the isometric projection (DirectionHelper.GetDirectionAB).
Direction directionAB(int ax, int ay, int bx, int by);

// Direction of a one-tile delta; None when the points are equal (DirectionHelper.CalculateDirection).
Direction directionOfDelta(int curX, int curY, int newX, int newY);

// Arrow keys to a world direction; None when nothing is held (DirectionHelper.DirectionFromKeyboardArrows).
Direction directionFromArrows(bool up, bool down, bool left, bool right);

// Screen-space direction from (x1, y1) to (toX, toY) (GameCursor.GetMouseDirection). The result is a
// screen compass index where 0 is straight up; subtract one (with 0 wrapping to 7) for the world
// direction, which is what mouseWalkDirection does.
int mouseDirection(int x1, int y1, int toX, int toY, int currentFacing);

// World direction to walk when the mouse is at (mouseX, mouseY) and the player is drawn at
// (centerX, centerY), both in screen pixels with y growing downward.
Direction mouseWalkDirection(int centerX, int centerY, int mouseX, int mouseY);

}  // namespace uo::movement
