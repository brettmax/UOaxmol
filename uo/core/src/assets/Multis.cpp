// SPDX-License-Identifier: BSD-2-Clause
#include "uo/assets/Multis.h"

#include "uo/io/BinaryReader.h"
#include "uo/io/UOFile.h"

#include <algorithm>

namespace uo::assets
{

std::vector<MultiComponent> Multis::decode(std::span<const std::uint8_t> raw, bool newFormat)
{
    std::size_t size  = newFormat ? kRecordNew : kRecordOld;
    std::size_t count = raw.size() / size;
    std::vector<MultiComponent> out;
    out.reserve(count);

    for (std::size_t i = 0; i < count; ++i)
    {
        io::BinaryReader r(raw.subspan(i * size, size));
        MultiComponent c;
        c.graphic = r.readU16LE();
        c.x       = r.readI16LE();
        c.y       = r.readI16LE();
        c.z       = r.readI16LE();
        c.flags   = r.readU32LE();
        c.visible = c.flags != 0;
        out.push_back(c);
    }
    return out;
}

MultiLoader::MultiLoader(std::unique_ptr<io::UOFile> file, bool newFormat) : _file(std::move(file)), _newFormat(newFormat) {}

MultiLoader::~MultiLoader() = default;

std::size_t MultiLoader::count() const
{
    return _file ? _file->count() : 0;
}

const std::vector<MultiComponent>& MultiLoader::components(std::uint16_t id) const
{
    if (auto it = _cache.find(id); it != _cache.end())
        return it->second;

    std::vector<MultiComponent> parts;
    if (_file)
        parts = Multis::decode(_file->read(id), _newFormat);
    return _cache.emplace(id, std::move(parts)).first->second;
}

MultiExtent MultiLoader::extent(std::uint16_t id) const
{
    MultiExtent e;
    for (const MultiComponent& c : components(id))
    {
        e.minX = std::min(e.minX, c.x);
        e.minY = std::min(e.minY, c.y);
        e.maxX = std::max(e.maxX, c.x);
        e.maxY = std::max(e.maxY, c.y);
    }
    return e;
}

}  // namespace uo::assets
