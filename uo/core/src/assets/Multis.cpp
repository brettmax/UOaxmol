// SPDX-License-Identifier: BSD-2-Clause
#include "uo/assets/Multis.h"

#include "uo/io/BinaryReader.h"

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

}  // namespace uo::assets
