// SPDX-License-Identifier: BSD-2-Clause
#include "uo/assets/Verdata.h"

#include "uo/io/BinaryReader.h"

#include <algorithm>

namespace uo::assets
{

bool Verdata::load(const std::string& path)
{
    _patches.clear();
    if (!_file.open(path))
        return false;

    io::BinaryReader r(_file.bytes());
    const std::int32_t count = r.readI32LE();

    // Servers ship broken verdata files: read the records that are actually there.
    if (count > 0)
    {
        const std::size_t n = std::min<std::size_t>(static_cast<std::size_t>(count), r.remaining() / 20);
        _patches.reserve(n);
        for (std::size_t i = 0; i < n; ++i)
        {
            VerdataPatch p;
            p.fileId   = r.readU32LE();
            p.blockId  = r.readU32LE();
            p.position = r.readU32LE();
            p.length   = r.readU32LE();
            p.extra    = r.readU32LE();
            _patches.push_back(p);
        }
    }
    return true;
}

}  // namespace uo::assets
