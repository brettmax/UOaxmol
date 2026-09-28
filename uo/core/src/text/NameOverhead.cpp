// SPDX-License-Identifier: BSD-2-Clause
// Ported from ClassicUO (NameOverheadGump.cs, Notoriety.cs, Profile.cs).

#include "uo/text/NameOverhead.h"

namespace uo::text
{

std::uint16_t notorietyHue(world::Notoriety notoriety)
{
    switch (notoriety)
    {
    case world::Notoriety::Innocent: return 0x005A;
    case world::Notoriety::Ally: return 0x0044;
    case world::Notoriety::Gray:
    case world::Notoriety::Criminal: return 0x03B2;
    case world::Notoriety::Enemy: return 0x0031;
    case world::Notoriety::Murderer: return 0x0023;
    case world::Notoriety::Invulnerable: return 0x0034;
    default: return 0;
    }
}

NameOverheadText nameOverheadText(const FontRenderer& fonts, std::uint8_t font, std::u16string_view name)
{
    NameOverheadText out{std::u16string(name), fonts.widthUnicode(font, name)};

    if (out.width > kNameOverheadWidth)
    {
        out.text  = fonts.textByWidthUnicode(font, name, kNameOverheadWidth, true);
        out.width = kNameOverheadWidth;
    }

    return out;
}

}  // namespace uo::text
