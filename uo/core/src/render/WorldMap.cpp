// SPDX-License-Identifier: BSD-2-Clause
// Ported from ClassicUO (Game/Map/Chunk.cs, GameObjects/Land.cs, Static.cs,
// StaticView.cs, LandView.cs, View.cs, Scenes/GameSceneDrawingSorting.cs).

#include "uo/render/WorldMap.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <unordered_set>

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

bool isTree(uint16_t graphic, const StaticTileData& data)
{
    // StaticFilters.Load's treeTiles, sorted.
    static constexpr uint16_t kTrees[] = {
        0x0C95, 0x0C96, 0x0C99, 0x0C9B, 0x0C9C, 0x0C9D, 0x0C9E, 0x0CA6, 0x0CA8, 0x0CAA, 0x0CAB, 0x0CC9, 0x0CCA,
        0x0CCB, 0x0CCC, 0x0CCD, 0x0CD0, 0x0CD3, 0x0CD6, 0x0CD8, 0x0CDA, 0x0CDD, 0x0CE0, 0x0CE3, 0x0CE6, 0x0CF8,
        0x0CFB, 0x0CFE, 0x0D01, 0x0D37, 0x0D38, 0x0D41, 0x0D42, 0x0D43, 0x0D44, 0x0D57, 0x0D58, 0x0D59, 0x0D5A,
        0x0D5B, 0x0D6E, 0x0D6F, 0x0D70, 0x0D71, 0x0D72, 0x0D84, 0x0D85, 0x0D86, 0x0D94, 0x0D98, 0x0D9C, 0x0DA0,
        0x0DA4, 0x0DA8, 0x12B6, 0x12B7, 0x12B8, 0x12B9, 0x12BA, 0x12BB, 0x12BC, 0x12BD,
    };
    return data.is(assets::TF_Impassable) && std::binary_search(std::begin(kTrees), std::end(kTrees), graphic);
}

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

            lc.graphic        = _seasons->landGraphic(_season, lc.graphic);
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

        const uint16_t graphic = _seasons->staticGraphic(_season, s.graphic);
        if (static_cast<int>(graphic) >= _tiles.itemCount())
        {
            continue;
        }
        StaticTileData data = _tiles.item(graphic);

        WorldObject st;
        st.kind             = ObjectKind::Static;
        st.graphic          = graphic;
        st.hue              = s.hue;
        st.x                = static_cast<uint16_t>(bx + s.x);
        st.y                = static_cast<uint16_t>(by + s.y);
        st.z                = s.z;
        st.allowedToDraw    = canDrawStatic(graphic, data);
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

void WorldMap::setSeason(SeasonId season, const SeasonTable* table)
{
    const SeasonTable* seasons = table ? table : &SeasonTable::defaults();
    if (season == _season && seasons == _seasons)
    {
        return;
    }
    _season  = season;
    _seasons = seasons;

    // Reload land and statics with the new graphics; carry dynamic objects over.
    std::vector<std::pair<int, int>> loaded;
    loaded.reserve(_blocks.size());
    for (const auto& [k, b] : _blocks)
    {
        loaded.emplace_back(b.blockX, b.blockY);
    }

    std::vector<WorldObject> dynamic;
    for (const auto& [bx, by] : loaded)
    {
        dynamic.clear();
        for (const Cell& cell : _blocks[key(bx, by)].cells)
        {
            for (const WorldObject& obj : cell)
            {
                if (obj.kind != ObjectKind::Land && obj.kind != ObjectKind::Static)
                {
                    dynamic.push_back(obj);
                }
            }
        }

        Block& block = loadBlock(bx, by);
        for (WorldObject& obj : dynamic)
        {
            insert(block.cells[((obj.y % kBlockSize) << 3) + (obj.x % kBlockSize)], std::move(obj));
        }
    }
}

int WorldMap::calculateNearZ(int defaultZ, int x, int y, int z) const
{
    // Map.CalculateNearZ, as a flood fill over roof tiles instead of recursion.
    struct Step
    {
        int x, y, z;
    };
    std::vector<Step> todo{{x, y, z}};
    std::unordered_set<uint64_t> visited;

    while (!todo.empty())
    {
        const Step s = todo.back();
        todo.pop_back();

        if (!visited.insert(key(s.x, s.y)).second)
        {
            continue;
        }

        const Cell* cell = cellAt(s.x, s.y);
        if (!cell)
        {
            continue;
        }

        const WorldObject* roof = nullptr;
        for (const WorldObject& obj : *cell)
        {
            if (obj.kind != ObjectKind::Static && obj.kind != ObjectKind::Multi)
            {
                continue;
            }
            if (_tiles.item(obj.graphic).is(assets::TF_Roof) && std::abs(s.z - obj.z) <= 6)
            {
                roof = &obj;
                break;
            }
        }

        if (!roof)
        {
            continue;
        }

        defaultZ = std::min<int>(defaultZ, roof->z);
        todo.push_back({s.x, s.y + 1, roof->z});
        todo.push_back({s.x, s.y - 1, roof->z});
        todo.push_back({s.x + 1, s.y, roof->z});
        todo.push_back({s.x - 1, s.y, roof->z});
    }

    return defaultZ;
}

ViewZ WorldMap::computeViewZ(int px, int py, int pz, bool drawRoofs) const
{
    // GameScene.UpdateMaxDrawZ.
    ViewZ v;
    v.hideRoofs = !drawRoofs;

    const int pz14 = pz + 14;
    const int pz16 = pz + 16;

    // Anything solid above the player's own tile (land over their head means a cave).
    if (const Cell* cell = cellAt(px, py))
    {
        for (const WorldObject& obj : *cell)
        {
            int tileZ = obj.z;

            if (obj.kind == ObjectKind::Land)
            {
                if (obj.land.stretched)
                {
                    tileZ = obj.land.averageZ;
                }
                if (pz16 <= tileZ)
                {
                    v.maxGroundZ = pz16;
                    v.maxZ       = pz16;
                    break;
                }
                continue;
            }

            if (obj.kind == ObjectKind::Mobile)
            {
                continue;
            }

            if (tileZ > pz14 && v.maxZ > tileZ)
            {
                const StaticTileData data = _tiles.item(obj.graphic);
                // Not transparent or foliage, and a roof only when it is also a surface.
                if (!data.is(assets::TF_Transparent) && !data.is(assets::TF_Foliage) &&
                    (!data.is(assets::TF_Roof) || data.is(assets::TF_Surface)))
                {
                    v.maxZ      = tileZ;
                    v.hideRoofs = true;
                }
            }
        }
    }

    int tempZ    = v.maxZ;
    v.maxGroundZ = v.maxZ;

    // A roof over the tile in front: the whole connected roof starts the cut.
    if (const Cell* cell = cellAt(px + 1, py + 1))
    {
        for (const WorldObject& obj : *cell)
        {
            if (obj.kind == ObjectKind::Mobile || obj.kind == ObjectKind::Land)
            {
                continue;
            }

            const int tileZ = obj.z;
            if (tileZ > pz14 && v.maxZ > tileZ)
            {
                const StaticTileData data = _tiles.item(obj.graphic);
                if (!data.is(assets::TF_Transparent) && !data.is(assets::TF_Surface) && data.is(assets::TF_Roof))
                {
                    v.maxZ       = tileZ;
                    v.maxGroundZ = calculateNearZ(tileZ, px + 1, py + 1, tileZ);
                    v.hideRoofs  = true;
                }
            }
        }
        tempZ = v.maxGroundZ;
    }

    v.maxZ = v.maxGroundZ;
    if (tempZ < pz16)
    {
        v.maxZ       = pz16;
        v.maxGroundZ = pz16;
    }
    v.maxGroundZ = tempZ;
    return v;
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

                // Bare branches in winter: foliage of statics and ground items is hidden.
                if ((obj.kind == ObjectKind::Static || obj.kind == ObjectKind::Item) && !foliageVisibleAtSeason(data, _season))
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

                // Trees and foliage stay visible inside the circle (StaticView.Draw).
                const bool cot = view.circleOfTransparency && !foliage && !isTree(obj.graphic, data) &&
                                 transparentTest(obj, data, view.playerZ + 5);

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
