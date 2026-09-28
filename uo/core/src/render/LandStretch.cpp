// SPDX-License-Identifier: BSD-2-Clause
// Ported from ClassicUO (ClassicUO.Client/Game/GameObjects/Land.cs).

#include "uo/render/LandStretch.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace uo::render
{

namespace
{

Vec3 cross(const Vec3& a, const Vec3& b)
{
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

Vec3 add(const Vec3& a, const Vec3& b)
{
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

// Sum of the four triangle normals around a vertex, normalised. Returns false
// (and a straight-up normal) when the neighbourhood is flat. Matches
// Land.CalculateNormal including its cross(v, u) argument order.
bool calculateNormal(int8_t tile, int8_t top, int8_t right, int8_t bottom, int8_t left, Vec3& normal)
{
    if (tile == top && tile == right && tile == bottom && tile == left)
    {
        normal = {0, 0, 1};
        return false;
    }

    auto dz = [tile](int8_t other) { return static_cast<float>((other - tile) * 4); };

    Vec3 ret = cross({-22, 22, dz(bottom)}, {-22, -22, dz(left)});
    ret      = add(ret, cross({22, 22, dz(right)}, {-22, 22, dz(bottom)}));
    ret      = add(ret, cross({22, -22, dz(top)}, {22, 22, dz(right)}));
    ret      = add(ret, cross({-22, -22, dz(left)}, {22, -22, dz(top)}));

    float len = std::sqrt(ret.x * ret.x + ret.y * ret.y + ret.z * ret.z);

    if (len > 0)
    {
        ret.x /= len;
        ret.y /= len;
        ret.z /= len;
    }

    normal = ret;
    return true;
}

}  // namespace

bool canStretchLand(const LandTileData& data, const ITileData& tiles)
{
    if (data.texId == 0 && (data.flags & assets::TF_Wet) != 0)
    {
        return false;
    }

    return tiles.hasTexmap(data.texId);
}

LandStretch computeLandStretch(const IMapSource& map, int x, int y, int8_t z, bool canStretch)
{
    LandStretch s;

    if (!canStretch)
    {
        s.averageZ = z;
        s.minZ     = z;
        return s;
    }

    /*  _____ _____
     * | top | rig |
     * |_____|_____|
     * | lef | bot |
     * |_____|_____|
     */
    int8_t zTop    = z;
    int8_t zRight  = map.landZ(x + 1, y);
    int8_t zLeft   = map.landZ(x, y + 1);
    int8_t zBottom = map.landZ(x + 1, y + 1);

    s.offsets.top    = zTop * 4;
    s.offsets.right  = zRight * 4;
    s.offsets.left   = zLeft * 4;
    s.offsets.bottom = zBottom * 4;

    if (std::abs(zTop - zBottom) <= std::abs(zLeft - zRight))
    {
        s.averageZ = static_cast<int8_t>((zTop + zBottom) >> 1);
    }
    else
    {
        s.averageZ = static_cast<int8_t>((zLeft + zRight) >> 1);
    }

    s.minZ = std::min({zTop, zRight, zLeft, zBottom});

    /*  _____ _____ _____ _____
     * |     | t10 | t20 |     |
     * |_____|_____|_____|_____|
     * | t01 |  z  | t21 | t31 |
     * |_____|_____|_____|_____|
     * | t02 | t12 | t22 | t32 |
     * |_____|_____|_____|_____|
     * |     | t13 | t23 |     |
     * |_____|_____|_____|_____|
     */
    int8_t t10 = map.landZ(x, y - 1);
    int8_t t20 = map.landZ(x + 1, y - 1);
    int8_t t01 = map.landZ(x - 1, y);
    int8_t t21 = zRight;
    int8_t t31 = map.landZ(x + 2, y);
    int8_t t02 = map.landZ(x - 1, y + 1);
    int8_t t12 = zLeft;
    int8_t t22 = zBottom;
    int8_t t32 = map.landZ(x + 2, y + 1);
    int8_t t13 = map.landZ(x, y + 2);
    int8_t t23 = map.landZ(x + 1, y + 2);

    s.stretched |= calculateNormal(z, t10, t21, t12, t01, s.normalTop);
    s.stretched |= calculateNormal(t21, t20, t31, t22, z, s.normalRight);
    s.stretched |= calculateNormal(t22, t21, t32, t23, t12, s.normalBottom);
    s.stretched |= calculateNormal(t12, z, t22, t13, t02, s.normalLeft);

    return s;
}

}  // namespace uo::render
