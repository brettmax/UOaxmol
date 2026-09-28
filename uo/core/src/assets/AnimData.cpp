// SPDX-License-Identifier: BSD-2-Clause
#include "uo/assets/AnimData.h"

#include "uo/io/BinaryReader.h"
#include "uo/io/MappedFile.h"

namespace uo::assets
{

bool AnimData::load(const std::string& path)
{
    io::MappedFile f;
    if (!f.open(path))
        return false;
    loadFromBytes(f.bytes());
    return true;
}

void AnimData::loadFromBytes(std::span<const std::uint8_t> bytes)
{
    _entries.clear();
    // Entry g sits at g * 68 + 4 * (g / 8 + 1): every group of eight carries a 4-byte header.
    for (std::uint32_t g = 0;; ++g)
    {
        std::size_t pos = static_cast<std::size_t>(g) * kEntryBytes + 4 * ((g >> 3) + 1);
        if (pos + kEntryBytes > bytes.size())
            break;
        io::BinaryReader r(bytes.subspan(pos, kEntryBytes));
        AnimDataEntry e;
        for (auto& f : e.frames)
            f = r.readI8();
        e.unknown       = r.readU8();
        e.frameCount    = r.readU8();
        e.frameInterval = r.readU8();
        e.frameStart    = r.readU8();
        _entries.push_back(e);
    }
}

}  // namespace uo::assets
