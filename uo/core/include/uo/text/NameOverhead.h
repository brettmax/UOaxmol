// SPDX-License-Identifier: BSD-2-Clause
// Ported from ClassicUO (NameOverheadGump.cs, Notoriety.cs, Profile.cs).

#pragma once

#include "uo/text/FontRenderer.h"
#include "uo/world/Types.h"

#include <cstdint>
#include <string>
#include <string_view>

namespace uo::text
{

// Name plates are at most this wide (Constants.OBJECT_HANDLES_GUMP_WIDTH).
inline constexpr int kNameOverheadWidth = 100;

// The name-plate hue for a notoriety, from the original client's default profile.
std::uint16_t notorietyHue(world::Notoriety notoriety);

// A name plate's text and wrap width: names wider than kNameOverheadWidth are cut to fit
// with "...", as NameOverheadGump.SetName does. Render it in unicode `font` with a black
// border, centered, at `width`.
struct NameOverheadText
{
    std::u16string text;
    int width = 0;
};

NameOverheadText nameOverheadText(const FontRenderer& fonts, std::uint8_t font, std::u16string_view name);

}  // namespace uo::text
