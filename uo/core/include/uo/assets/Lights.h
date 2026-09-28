// SPDX-License-Identifier: BSD-2-Clause
// Part of the AxmolUO client core. Ported from ClassicUO.Assets (LightsLoader).
#pragma once

#include "uo/assets/Image.h"

#include <cstdint>
#include <span>

namespace uo::assets
{

// light.mul + lightidx.mul: one byte per pixel, width/height in the idx "extra" word. Values
// run -31..31 with negatives stored bit-inverted; the client draws the magnitude as grey,
// blended additively, and zero as transparent.
struct Lights
{
    static Image decode(std::span<const std::uint8_t> raw, int width, int height);
};

}  // namespace uo::assets
