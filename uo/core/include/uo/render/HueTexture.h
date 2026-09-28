// SPDX-License-Identifier: BSD-2-Clause
// Part of the AxmolUO client core. Ported from ClassicUO (Client.Load hue sampler setup).
//
// The hue lookup texture the world shader samples: 16 hues per row, each hue a
// 32-texel ramp from dark to light, 1024 rows. Hue h (1-based) is texels
// [(h - 1) * 32, h * 32) in row-major order, so assets::Hues::buildHueTexture()
// output is already laid out right and only needs padding to full size.

#pragma once

#include <cstdint>
#include <span>
#include <vector>

namespace uo::render
{

inline constexpr int kHueRampWidth     = 32;
inline constexpr int kHuesPerRow       = 16;
inline constexpr int kHueTextureWidth  = kHueRampWidth * kHuesPerRow;  // 512
inline constexpr int kHueTextureHeight = 1024;

// Pads (or truncates) linear hue ramps to kHueTextureWidth x kHueTextureHeight RGBA8.
std::vector<std::uint32_t> packHueTexture(std::span<const std::uint32_t> ramps);

}  // namespace uo::render
