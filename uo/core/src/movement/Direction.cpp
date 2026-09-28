// SPDX-License-Identifier: BSD-2-Clause
// Ported from ClassicUO (Game/Data/Direction.cs, Game/GameCursor.cs).

#include "uo/movement/Direction.h"

#include <cstdlib>

namespace uo::movement
{

Direction directionAB(int ax, int ay, int bx, int by)
{
    const int dx = ax - bx;
    const int dy = ay - by;
    const int rx = (dx - dy) * 44;
    const int ry = (dx + dy) * 44;
    const int absX = std::abs(rx);
    const int absY = std::abs(ry);

    if ((absY >> 1) - absX >= 0)
    {
        return ry > 0 ? Direction::Up : Direction::Down;
    }
    if ((absX >> 1) - absY >= 0)
    {
        return rx > 0 ? Direction::Left : Direction::Right;
    }
    if (rx >= 0 && ry >= 0)
    {
        return Direction::West;
    }
    if (rx >= 0 && ry < 0)
    {
        return Direction::South;
    }
    if (rx < 0 && ry < 0)
    {
        return Direction::East;
    }
    return Direction::North;
}

Direction directionOfDelta(int curX, int curY, int newX, int newY)
{
    const int dx = newX - curX;
    const int dy = newY - curY;

    if (dx > 0)
    {
        if (dy > 0)
        {
            return Direction::Down;
        }
        return dy == 0 ? Direction::East : Direction::Right;
    }
    if (dx == 0)
    {
        if (dy > 0)
        {
            return Direction::South;
        }
        return dy == 0 ? kDirectionNone : Direction::North;
    }
    if (dy > 0)
    {
        return Direction::Left;
    }
    return dy == 0 ? Direction::West : Direction::Up;
}

Direction directionFromArrows(bool up, bool down, bool left, bool right)
{
    if (up)
    {
        return left ? Direction::West : right ? Direction::North : Direction::Up;
    }
    if (down)
    {
        return left ? Direction::South : right ? Direction::East : Direction::Down;
    }
    if (left)
    {
        return Direction::Left;
    }
    if (right)
    {
        return Direction::Right;
    }
    return kDirectionNone;
}

namespace
{
int sgn(int v) { return (v > 0 ? 1 : 0) - (v < 0 ? 1 : 0); }
}  // namespace

int mouseDirection(int x1, int y1, int toX, int toY, int currentFacing)
{
    int shiftX = toX - x1;
    int shiftY = toY - y1;
    int hash = 100 * (sgn(shiftX) + 2) + 10 * (sgn(shiftY) + 2);

    if (shiftX != 0 && shiftY != 0)
    {
        shiftX = std::abs(shiftX);
        shiftY = std::abs(shiftY);

        if (shiftY * 5 <= shiftX * 2)
        {
            hash += 1;
        }
        else if (shiftY * 2 >= shiftX * 5)
        {
            hash += 3;
        }
        else
        {
            hash += 2;
        }
    }
    else if (shiftX == 0 && shiftY == 0)
    {
        return currentFacing;
    }

    switch (hash)
    {
    case 111: return toByte(Direction::West);
    case 112: return toByte(Direction::Up);
    case 113: return toByte(Direction::North);
    case 120: return toByte(Direction::West);
    case 131: return toByte(Direction::West);
    case 132: return toByte(Direction::Left);
    case 133: return toByte(Direction::South);
    case 210: return toByte(Direction::North);
    case 230: return toByte(Direction::South);
    case 311: return toByte(Direction::East);
    case 312: return toByte(Direction::Right);
    case 313: return toByte(Direction::North);
    case 320: return toByte(Direction::East);
    case 331: return toByte(Direction::East);
    case 332: return toByte(Direction::Down);
    case 333: return toByte(Direction::South);
    default: return currentFacing;
    }
}

Direction mouseWalkDirection(int centerX, int centerY, int mouseX, int mouseY)
{
    int facing = mouseDirection(centerX, centerY, mouseX, mouseY, 1);
    if (facing == 0)
    {
        facing = 8;
    }
    return toDirection(facing - 1);
}

}  // namespace uo::movement
