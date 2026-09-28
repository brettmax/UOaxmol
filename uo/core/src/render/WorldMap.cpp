// SPDX-License-Identifier: BSD-2-Clause
// Ported from ClassicUO (Game/Map/Chunk.cs, GameObjects/Land.cs, Static.cs,
// StaticView.cs, LandView.cs, View.cs, Scenes/GameSceneDrawingSorting.cs).

#include "uo/render/WorldMap.h"

#include <algorithm>
#include <cctype>

namespace uo::render
{

namespace
{

bool startsWithNoDraw(std::string_view name)
{
    constexpr std::string_view kNoDraw = "nodraw";

    if (name.size() < kNoDraw.size())
    {
        return false;
    }

    for (size_t i = 0; i < kNoDraw.size(); ++i)
    {
        if (std::tolower(static_cast<unsigned char>(name[i])) != kNoDraw[i])
        {
            return false;
        }
    }

    return true;
}

// Static.Create: which statics the circle of transparency may cut through.
bool computeCanBeTransparent(const StaticTileData& d)
{
    if (d.height > 5 || d.height == 0)
    {
        return true;
    }

    if (d.is(assets::TF_Roof) || (d.is(assets::TF_Surface) && d.is(assets::TF_Background)) || d.is(assets::TF_Wall))
    {
        return true;
    }

    return d.height == 5 && d.is(assets::TF_Surface) && !d.is(assets::TF_Background);
}

// StaticView.TransparentTest
bool transparentTest(const WorldObject& obj, const StaticTileData& d, int z)
{
    if (obj.z <= z - d.height)
    {
        return false;
    }

    if (z < obj.z && !obj.canBeTransparent)
    {
        return false;
    }

    return true;
}

}  // namespace

bool canDrawStatic(uint16_t g, const StaticTileData& data, bool gargoyle)
{
    switch (g)
    {
    case 0x0001:
    case 0x21BC:
    case 0xA1FE:
    case 0xA1FF:
    case 0xA200:
    case 0xA201:
        return false;

    case 0x9E4C:
    case 0x9E64:
    case 0x9E65:
    case 0x9E7D:
        return !data.is(assets::TF_Background) && !data.is(assets::TF_Surface);
    }

    if (g == 0x63D3)
    {
        return false;
    }

    if (g >= 0x2198 && g <= 0x21A4)
    {
        return false;
    }

    // Hacky way ClassicUO skips "nodraw" tiles.
    if (startsWithNoDraw(data.name))
    {
        return false;
    }

    return !data.is(assets::TF_NoDiagonal) || (data.is(assets::TF_Animation) && gargoyle);
}

Priority computePriority(const WorldObject& obj, const ITileData& tiles)
{
    int priorityZ = obj.z;
    int8_t state  = -1;
    bool useItemData = false;

    switch (obj.kind)
    {
    case ObjectKind::Land:
        priorityZ = obj.land.stretched ? obj.land.averageZ - 1 : priorityZ - 1;
        priorityZ -= 1;
        state = 0;
        break;

    case ObjectKind::Mobile:
    case ObjectKind::Corpse:
        priorityZ++;
        break;

    case ObjectKind::Effect:
        priorityZ += 2;
        break;

    case ObjectKind::Multi:
        state = 1;

        if ((obj.multiState & MULTI_GENERIC_INTERNAL) != 0)
        {
            priorityZ--;
            break;
        }

        if ((obj.multiState & MULTI_PREVIEW) != 0)
        {
            state = 2;
            priorityZ++;
        }

        useItemData = true;
        break;

    case ObjectKind::Static:
    case ObjectKind::Item:
        useItemData = true;
        break;
    }

    if (useItemData)
    {
        StaticTileData data = tiles.item(obj.graphic);

        if (data.is(assets::TF_Background))
        {
            priorityZ--;
        }

        if (data.height != 0)
        {
            priorityZ++;
        }

        if (data.is(assets::TF_MultiMovable))
        {
            priorityZ++;
        }
    }

    return {static_cast<int16_t>(priorityZ), state};
}

void WorldMap::insert(Cell& cell, WorldObject obj)
{
    Priority p    = computePriority(obj, _tiles);
    obj.priorityZ = p.priorityZ;

    // Chunk.AddGameObject: walk from the head and insert after the last object
    // that must stay in front of us. Land goes before equal-priority objects;
    // multi parts go after land but before other equal-priority objects.
    size_t pos = 0;

    for (size_t i = 0; i < cell.size(); ++i)
    {
        int test = cell[i].priorityZ;

        if (test > p.priorityZ ||
            (test == p.priorityZ && (p.state == 0 || (p.state == 1 && cell[i].kind != ObjectKind::Land))))
        {
            break;
        }

        pos = i + 1;
    }

    cell.insert(cell.begin() + static_cast<std::ptrdiff_t>(pos), std::move(obj));
}

WorldMap::Block& WorldMap::loadBlock(int blockX, int blockY)
{
    Block& block = _blocks[key(blockX, blockY)];
    block.blockX = blockX;
    block.blockY = blockY;

    for (Cell& c : block.cells)
    {
        c.clear();
    }

    const int bx = blockX * kBlockSize;
    const int by = blockY * kBlockSize;

    for (int y = 0; y < kBlockSize; ++y)
    {
        for (int x = 0; x < kBlockSize; ++x)
        {
            LandCell lc;

            if (!_map.land(bx + x, by + y, lc))
            {
                continue;
            }

            LandTileData data = _tiles.land(lc.graphic);

            WorldObject land;
            land.kind          = ObjectKind::Land;
            land.graphic       = lc.graphic;
            land.x             = static_cast<uint16_t>(bx + x);
            land.y             = static_cast<uint16_t>(by + y);
            land.z             = lc.z;
            land.allowedToDraw = lc.graphic > 2;
            land.land          = computeLandStretch(_map, bx + x, by + y, lc.z, canStretchLand(data, _tiles));

            insert(block.cells[(y << 3) + x], std::move(land));
        }
    }

    std::vector<StaticEntry> statics;
    _map.statics(blockX, blockY, statics);

    for (const StaticEntry& s : statics)
    {
        if (s.graphic == 0 || s.graphic == 0xFFFF || s.x >= kBlockSize || s.y >= kBlockSize)
        {
            continue;
        }

        if (static_cast<int>(s.graphic) >= _tiles.itemCount())
        {
            continue;
        }

        StaticTileData data = _tiles.item(s.graphic);

        WorldObject st;
        st.kind             = ObjectKind::Static;
        st.graphic          = s.graphic;
        st.hue              = s.hue;
        st.x                = static_cast<uint16_t>(bx + s.x);
        st.y                = static_cast<uint16_t>(by + s.y);
        st.z                = s.z;
        st.allowedToDraw    = canDrawStatic(s.graphic, data);
        st.canBeTransparent = computeCanBeTransparent(data);

        insert(block.cells[(s.y << 3) + s.x], std::move(st));
    }

    return block;
}

void WorldMap::unloadBlock(int blockX, int blockY)
{
    _blocks.erase(key(blockX, blockY));
}

const WorldMap::Block* WorldMap::findBlock(int blockX, int blockY) const
{
    auto it = _blocks.find(key(blockX, blockY));
    return it == _blocks.end() ? nullptr : &it->second;
}

const WorldMap::Cell* WorldMap::cellAt(int x, int y) const
{
    if (x < 0 || y < 0)
    {
        return nullptr;
    }

    const Block* b = findBlock(x / kBlockSize, y / kBlockSize);
    return b ? &b->cells[((y % kBlockSize) << 3) + (x % kBlockSize)] : nullptr;
}

void WorldMap::ensureLoaded(const ViewParams& view)
{
    const int minBX = std::max(0, view.minTileX) / kBlockSize;
    const int minBY = std::max(0, view.minTileY) / kBlockSize;
    const int maxBX = std::min(view.maxTileX, _map.width() - 1) / kBlockSize;
    const int maxBY = std::min(view.maxTileY, _map.height() - 1) / kBlockSize;

    for (int by = minBY; by <= maxBY; ++by)
    {
        for (int bx = minBX; bx <= maxBX; ++bx)
        {
            if (!findBlock(bx, by))
            {
                loadBlock(bx, by);
            }
        }
    }
}

bool WorldMap::addObject(WorldObject obj)
{
    auto it = _blocks.find(key(obj.x / kBlockSize, obj.y / kBlockSize));

    if (it == _blocks.end())
    {
        return false;
    }

    insert(it->second.cells[((obj.y % kBlockSize) << 3) + (obj.x % kBlockSize)], std::move(obj));
    return true;
}

bool WorldMap::removeObject(std::uint32_t serial, int x, int y)
{
    if (serial == 0 || x < 0 || y < 0)
    {
        return false;
    }

    auto it = _blocks.find(key(x / kBlockSize, y / kBlockSize));

    if (it == _blocks.end())
    {
        return false;
    }

    Cell& cell = it->second.cells[((y % kBlockSize) << 3) + (x % kBlockSize)];

    for (auto o = cell.begin(); o != cell.end(); ++o)
    {
        if (o->serial == serial)
        {
            cell.erase(o);
            return true;
        }
    }

    return false;
}

void WorldMap::buildDrawList(const ViewParams& view, std::vector<DrawItem>& out, IMobileDrawSource* mobiles) const
{
    out.clear();

    struct Keyed
    {
        float depth;
        int x;
        uint32_t seq;
        DrawItem item;
    };

    std::vector<Keyed> keyed;
    uint32_t seq = 0;
    std::vector<DrawItem> parts;

    const int minX = std::max(0, view.minTileX);
    const int minY = std::max(0, view.minTileY);

    for (int y = minY; y <= view.maxTileY; ++y)
    {
        for (int x = minX; x <= view.maxTileX; ++x)
        {
            const Cell* cell = cellAt(x, y);

            if (!cell)
            {
                continue;
            }

            for (const WorldObject& obj : *cell)
            {
                // RenderLists: a tile stops at the first object above the ground cap.
                if (obj.priorityZ > view.maxGroundZ)
                {
                    break;
                }

                if (!obj.allowedToDraw)
                {
                    continue;
                }

                DrawItem item;
                item.graphic = obj.graphic;
                item.object  = &obj;
                isoScreenPosition(obj.x, obj.y, obj.z, view.offsetX, view.offsetY, item.screenX, item.screenY);

                // The +0.5 ClassicUO's batcher adds to every sprite; shadows get +0.25.
                const float depth = depthKey(obj.x, obj.y, obj.priorityZ) + 0.5f;
                item.depth        = depth;

                if (obj.kind == ObjectKind::Land)
                {
                    const bool stretched = obj.land.stretched;
                    item.type = stretched ? DrawType::LandStretched : DrawType::LandFlat;
                    item.hue  = makeLandHueVector(view.overrideHue ? view.overrideHue : obj.hue, stretched);

                    if (stretched)
                    {
                        // Corner offsets already include Z (LandView.Draw).
                        item.screenY += obj.z << 2;
                    }

                    keyed.push_back({depth, x, seq++, item});
                    continue;
                }

                if (obj.z >= view.maxZ)
                {
                    continue;
                }

                if (obj.kind == ObjectKind::Mobile)
                {
                    // A body id is not an item graphic: without a source there is nothing to draw.
                    if (!mobiles)
                    {
                        continue;
                    }

                    parts.clear();
                    mobiles->appendMobile(obj, item, parts);

                    for (DrawItem& part : parts)
                    {
                        part.object = &obj;
                        part.depth  = depth;
                        keyed.push_back({depth, x, seq++, part});
                    }
                    continue;
                }

                StaticTileData data = _tiles.item(obj.graphic);

                if (view.hideRoofs && data.is(assets::TF_Roof))
                {
                    continue;
                }

                float alpha = obj.alpha / 255.0f;

                if (data.is(assets::TF_Translucent))
                {
                    alpha = std::min(alpha, 178 / 255.0f);
                }

                uint16_t hue = obj.hue;
                bool partial = data.is(assets::TF_PartialHue);

                if (view.overrideHue != 0)
                {
                    hue     = view.overrideHue;
                    partial = false;
                }

                const bool foliage = data.is(assets::TF_Foliage);

                // Foliage stays visible inside the circle (trees too, pending the
                // StaticFilters tree table).
                const bool cot = view.circleOfTransparency && !foliage && transparentTest(obj, data, view.playerZ + 5);

                if (view.staticShadows && foliage)
                {
                    DrawItem shadow = item;
                    shadow.type     = DrawType::Shadow;
                    shadow.hue      = HueVector{0, SHADER_SHADOW, 1};
                    shadow.depth    = depth - 0.25f;
                    keyed.push_back({shadow.depth, x, seq++, shadow});
                }

                item.type = DrawType::Static;
                item.hue  = makeHueVector(hue, partial, alpha, false, false, cot);
                keyed.push_back({depth, x, seq++, item});
            }
        }
    }

    std::sort(keyed.begin(), keyed.end(), [](const Keyed& a, const Keyed& b) {
        if (a.depth != b.depth)
        {
            return a.depth < b.depth;
        }

        if (a.x != b.x)
        {
            return a.x < b.x;
        }

        return a.seq < b.seq;
    });

    out.reserve(keyed.size());

    for (Keyed& k : keyed)
    {
        out.push_back(k.item);
    }
}

}  // namespace uo::render
