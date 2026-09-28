// SPDX-License-Identifier: BSD-2-Clause
// Ported from ClassicUO (Game/GameObjects/Entity.cs, Mobile.cs, Game/Data/Direction.cs).
#include "uo/world/Entity.h"

namespace uo::world
{

Direction calculateDirection(int curX, int curY, int newX, int newY) noexcept
{
    const int dx = newX - curX;
    const int dy = newY - curY;

    if (dx > 0)
    {
        if (dy > 0)
            return Direction::Down;
        return dy == 0 ? Direction::East : Direction::Right;
    }

    if (dx == 0)
    {
        if (dy > 0)
            return Direction::South;
        return dy == 0 ? Direction::None : Direction::North;
    }

    if (dy > 0)
        return Direction::Left;
    return dy == 0 ? Direction::West : Direction::Up;
}

void Entity::fixHue(uint16_t h) noexcept
{
    uint16_t fixed = uint16_t(h & 0x3FFF);

    if (fixed != 0)
    {
        if (fixed >= 0x0BB8)
            fixed = 1;
        fixed = uint16_t(fixed | (h & 0xC000));
    }
    else
    {
        fixed = uint16_t(h & 0x8000);
    }

    hue = fixed;
}

bool Mobile::isDead() const noexcept
{
    return graphic == 0x0192 || graphic == 0x0193 || (graphic >= 0x025F && graphic <= 0x0260) || graphic == 0x02B6 ||
           graphic == 0x02B7 || _isDead;
}

bool Mobile::isPoisoned(ClientVersion version) const noexcept
{
    return version >= versions::CV_7000 ? _saPoisoned : (flagBits & flags::Poisoned) != 0;
}

bool Mobile::isFlying(ClientVersion version) const noexcept
{
    return version >= versions::CV_7000 && (flagBits & flags::Poisoned) != 0;
}

bool Mobile::isHuman() const noexcept
{
    const uint16_t g = graphic;
    return (g >= 0x0190 && g <= 0x0193) || (g >= 0x00B7 && g <= 0x00BA) || (g >= 0x025D && g <= 0x0260) ||
           g == 0x029A || g == 0x029B || g == 0x02B6 || g == 0x02B7 || g == 0x03DB || g == 0x03DF || g == 0x03E2 ||
           g == 0x02E8 || g == 0x02E9 || g == 0x04E5;
}

void Mobile::endPosition(int& ex, int& ey, int8_t& ez, Direction& edir) const noexcept
{
    if (steps.empty())
    {
        ex = x;
        ey = y;
        ez = z;
        edir = direction;
    }
    else
    {
        const auto& s = steps.back();
        ex = s.x;
        ey = s.y;
        ez = s.z;
        edir = s.direction;
    }
}

bool Mobile::enqueueStep(int nx, int ny, int8_t nz, Direction dir, bool run)
{
    if (int(steps.size()) >= kMaxStepCount)
        return false;

    int ex, ey;
    int8_t ez;
    Direction edir;
    endPosition(ex, ey, ez, edir);

    if (ex == nx && ey == ny && ez == nz && edir == dir)
        return true;

    const Direction moveDir = calculateDirection(ex, ey, nx, ny);

    if (moveDir != Direction::None)
    {
        if (moveDir != edir)
            steps.push_back({ex, ey, ez, moveDir, run});

        steps.push_back({nx, ny, nz, moveDir, run});
    }

    if (moveDir != dir)
        steps.push_back({nx, ny, nz, dir, run});

    return true;
}

} // namespace uo::world
