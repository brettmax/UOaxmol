// SPDX-License-Identifier: BSD-2-Clause
// Part of the AxmolUO client core. Ported from ClassicUO.Utility (HuesHelper).
#pragma once

#include <cstdint>

namespace uo::assets
{

// UO stores colours as 15-bit ARGB1555 without the alpha bit: 0RRRRRGGGGGBBBBB.
// Pixels are expanded to 32-bit values whose little-endian byte order is R, G, B, A, which is
// Axmol's backend::PixelFormat::RGBA8 and can be handed to Texture2D::initWithData as-is.
inline constexpr std::uint8_t kColor5To8[32] = {0x00, 0x08, 0x10, 0x18, 0x20, 0x29, 0x31, 0x39, 0x41, 0x4A, 0x52,
                                                0x5A, 0x62, 0x6A, 0x73, 0x7B, 0x83, 0x8B, 0x94, 0x9C, 0xA4, 0xAC,
                                                0xB4, 0xBD, 0xC5, 0xCD, 0xD5, 0xDE, 0xE6, 0xEE, 0xF6, 0xFF};

// Opaque-less colour: alpha byte is 0; callers OR in 0xFF000000 for opaque pixels.
constexpr std::uint32_t color16To32(std::uint16_t c)
{
    return static_cast<std::uint32_t>(kColor5To8[(c >> 10) & 0x1F]) |
           (static_cast<std::uint32_t>(kColor5To8[(c >> 5) & 0x1F]) << 8) |
           (static_cast<std::uint32_t>(kColor5To8[c & 0x1F]) << 16);
}

constexpr std::uint32_t kOpaque = 0xFF000000u;

}  // namespace uo::assets
