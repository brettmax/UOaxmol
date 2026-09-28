// SPDX-License-Identifier: BSD-2-Clause
#include "uo/render/WorldSource.h"

#include "uo/assets/Map.h"
#include "uo/assets/Texmaps.h"

namespace uo::render
{

int AssetsWorldSource::width() const
{
    return _map.width();
}

int AssetsWorldSource::height() const
{
    return _map.height();
}

bool AssetsWorldSource::land(int x, int y, LandCell& out) const
{
    if (x < 0 || y < 0 || x >= _map.width() || y >= _map.height())
    {
        return false;
    }

    assets::LandCell c = _map.land(x, y);
    out.graphic        = static_cast<std::uint16_t>(c.tileId & 0x3FFF);
    out.z              = c.z;
    return true;
}

void AssetsWorldSource::statics(int blockX, int blockY, std::vector<StaticEntry>& out) const
{
    for (const assets::StaticCell& s : _map.staticsBlock(blockX, blockY))
    {
        out.push_back({s.graphic, s.x, s.y, s.z, s.hue});
    }
}

LandTileData AssetsWorldSource::land(std::uint16_t graphic) const
{
    const assets::LandTile* t = _tiles.landTile(graphic);
    return t ? LandTileData{t->flags, t->texId} : LandTileData{};
}

StaticTileData AssetsWorldSource::item(std::uint16_t graphic) const
{
    const assets::StaticTile* t = _tiles.staticTile(graphic);
    return t ? StaticTileData{t->flags, t->height, t->name} : StaticTileData{};
}

int AssetsWorldSource::itemCount() const
{
    return static_cast<int>(_tiles.statics().size());
}

bool AssetsWorldSource::hasTexmap(std::uint16_t texId) const
{
    return _texmaps && _texmaps->has(texId);
}

}  // namespace uo::render
