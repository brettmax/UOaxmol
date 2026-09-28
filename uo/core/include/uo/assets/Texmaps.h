// SPDX-License-Identifier: BSD-2-Clause
// Part of the AxmolUO client core. Ported from ClassicUO.Assets (TexmapsLoader).
#pragma once

#include "uo/assets/Image.h"

#include <cstddef>
#include <cstdint>
#include <span>

namespace uo::assets
{

// texmaps.mul + texidx.mul: square ground textures that stretched land tiles are drawn with.
// An entry of 0x2000 bytes is 64x64, anything else is 128x128; every pixel is opaque.
struct Texmaps
{
    static int sizeFor(std::size_t length) { return length == 0x2000 ? 64 : 128; }
    static Image decode(std::span<const std::uint8_t> raw);
};

}  // namespace uo::assets
