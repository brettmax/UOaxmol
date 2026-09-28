// SPDX-License-Identifier: BSD-2-Clause
// Ported from ClassicUO (Game/SelectedObject.cs, GameObjects/*.CheckMouseSelection).

#include "uo/render/Pick.h"

#include "uo/assets/Art.h"
#include "uo/assets/Image.h"

namespace uo::render
{

namespace
{

// SelectedObject.IsPointInLand: the 44x44 land diamond anchored at (sx, sy).
bool inDiamond(int sx, int sy, int x, int y)
{
    const int dx = x - sx, dy = y - sy;
    if (dx < 0 || dy < 0 || dx >= 44 || dy >= 44)
    {
        return false;
    }
    // Centre the test on texel centres: |2dx - 43| + |2dy - 43| <= 44.
    const int ax = 2 * dx - 43, ay = 2 * dy - 43;
    return (ax < 0 ? -ax : ax) + (ay < 0 ? -ay : ay) <= 44;
}

float edge(float ax, float ay, float bx, float by, float px, float py)
{
    return (bx - ax) * (py - ay) - (by - ay) * (px - ax);
}

bool inTriangle(float ax, float ay, float bx, float by, float cx, float cy, float px, float py)
{
    const float e0 = edge(ax, ay, bx, by, px, py);
    const float e1 = edge(bx, by, cx, cy, px, py);
    const float e2 = edge(cx, cy, ax, ay, px, py);
    return (e0 >= 0 && e1 >= 0 && e2 >= 0) || (e0 <= 0 && e1 <= 0 && e2 <= 0);
}

// SelectedObject.IsPointInStretchedLand: the quad WorldGeometry emits.
bool inStretched(const DrawItem& item, int x, int y)
{
    const YOffsets& o = item.object->land.offsets;
    const float sx = static_cast<float>(item.screenX), sy = static_cast<float>(item.screenY);
    const float px = x + 0.5f, py = y + 0.5f;

    const float tx = sx + 22, ty = sy - o.top;
    const float rx = sx + 44, ry = sy + (22 - o.right);
    const float lx = sx, ly = sy + (22 - o.left);
    const float bx = sx + 22, by = sy + (44 - o.bottom);

    return inTriangle(tx, ty, rx, ry, lx, ly, px, py) || inTriangle(rx, ry, bx, by, lx, ly, px, py);
}

}  // namespace

ArtHitTest::ArtHitTest(const assets::Art& art) : _art(art) {}
ArtHitTest::~ArtHitTest() = default;

const assets::Image* ArtHitTest::image(uint16_t graphic)
{
    auto it = _cache.find(graphic);
    if (it == _cache.end())
    {
        it = _cache.emplace(graphic, std::make_unique<assets::Image>(_art.statik(graphic))).first;
    }
    return it->second->empty() ? nullptr : it->second.get();
}

bool ArtHitTest::itemSize(uint16_t graphic, int& width, int& height)
{
    const assets::Image* img = image(graphic);
    if (!img)
    {
        return false;
    }
    width  = img->width;
    height = img->height;
    return true;
}

bool ArtHitTest::itemOpaque(uint16_t graphic, int x, int y)
{
    const assets::Image* img = image(graphic);
    if (!img || x < 0 || y < 0 || x >= img->width || y >= img->height)
    {
        return false;
    }
    return (img->at(x, y) >> 24) != 0;
}

bool hitTest(const DrawItem& item, const ITileData& tiles, IArtHitTest& art, int x, int y, const PickCircle& circle)
{
    if (!item.object || item.hue.alpha <= 0.0f)
    {
        return false;
    }

    switch (item.type)
    {
    case DrawType::LandFlat:
        return inDiamond(item.screenX, item.screenY, x, y);

    case DrawType::LandStretched:
        if (!tiles.hasTexmap(tiles.land(item.graphic).texId))
        {
            // Drawn as flat art raised by z (see buildWorldGeometry).
            return inDiamond(item.screenX, item.screenY - (item.object->z << 2), x, y);
        }
        return inStretched(item, x, y);

    case DrawType::Static:
    {
        // Alpha above 1 marks pixels the circle of transparency discards.
        if (item.hue.alpha > 1.0f && circle.radius > 0)
        {
            const long dx = x - circle.centerX, dy = y - circle.centerY;
            if (dx * dx + dy * dy <= static_cast<long>(circle.radius) * circle.radius)
            {
                return false;
            }
        }

        int w, h;
        if (!art.itemSize(item.graphic, w, h))
        {
            return false;
        }
        int ox, oy;
        staticDrawOrigin(item.screenX, item.screenY, w, h, ox, oy);
        return art.itemOpaque(item.graphic, x - ox, y - oy);
    }

    case DrawType::Shadow:
    case DrawType::Mobile:  // WorldScene::entityAt picks mobiles itself for now
        return false;
    }
    return false;
}

const DrawItem* pick(const std::vector<DrawItem>& drawList, const ITileData& tiles, IArtHitTest& art, int x, int y, const PickCircle& circle)
{
    for (auto it = drawList.rbegin(); it != drawList.rend(); ++it)
    {
        if (hitTest(*it, tiles, art, x, y, circle))
        {
            return &*it;
        }
    }
    return nullptr;
}

}  // namespace uo::render
