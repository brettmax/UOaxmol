// SPDX-License-Identifier: BSD-2-Clause
#include "uo/io/Verdata.h"

#include "uo/io/BinaryReader.h"

namespace uo::io
{

bool Verdata::load(const std::string& path)
{
    _patches.clear();
    _byKey.clear();
    if (!_file.open(path) || _file.size() < 4)
        return false;

    BinaryReader r(_file.bytes());
    std::int32_t count = r.readI32LE();
    if (count < 0 || static_cast<std::size_t>(count) > (_file.size() - 4) / 20)
        return false;

    _patches.reserve(count);
    for (std::int32_t i = 0; i < count; ++i)
    {
        Patch p;
        p.fileId   = r.readU32LE();
        p.blockId  = r.readU32LE();
        p.position = r.readU32LE();
        p.length   = r.readU32LE();
        p.extra    = r.readU32LE();
        _byKey[(static_cast<std::uint64_t>(p.fileId) << 32) | p.blockId] = _patches.size();
        _patches.push_back(p);
    }
    return true;
}

const Verdata::Patch* Verdata::find(std::uint32_t fileId, std::uint32_t blockId) const
{
    auto it = _byKey.find((static_cast<std::uint64_t>(fileId) << 32) | blockId);
    return it == _byKey.end() ? nullptr : &_patches[it->second];
}

}  // namespace uo::io
