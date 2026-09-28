// SPDX-License-Identifier: BSD-2-Clause
#include "uo/net/PacketWriter.h"

namespace uo::net
{

PacketWriter& PacketWriter::unicodeBE(std::string_view s)
{
    std::size_t i = 0;
    while (i < s.size())
    {
        std::uint32_t cp = static_cast<unsigned char>(s[i]);
        int extra        = cp >= 0xF0 ? 3 : cp >= 0xE0 ? 2 : cp >= 0xC0 ? 1 : 0;
        cp &= extra == 3 ? 0x07 : extra == 2 ? 0x0F : extra == 1 ? 0x1F : 0x7F;
        ++i;
        for (int k = 0; k < extra && i < s.size(); ++k, ++i)
            cp = (cp << 6) | (static_cast<unsigned char>(s[i]) & 0x3F);

        if (cp >= 0x10000)
        {
            cp -= 0x10000;
            u16(static_cast<std::uint16_t>(0xD800 + (cp >> 10)));
            u16(static_cast<std::uint16_t>(0xDC00 + (cp & 0x3FF)));
        }
        else
        {
            u16(static_cast<std::uint16_t>(cp));
        }
    }
    return u16(0);
}

}  // namespace uo::net
