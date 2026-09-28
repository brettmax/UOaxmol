// SPDX-License-Identifier: BSD-2-Clause
#pragma once

#include <cstdint>
#include <vector>

namespace uo::assets
{

// A decoded sprite: width * height RGBA8 pixels (see Color.h for the byte order), row-major,
// top-left origin. Transparent pixels are 0.
struct Image
{
    int width  = 0;
    int height = 0;
    std::vector<std::uint32_t> pixels;

    bool empty() const { return width <= 0 || height <= 0 || pixels.empty(); }
    std::uint32_t at(int x, int y) const { return pixels[static_cast<std::size_t>(y) * width + x]; }
};

}  // namespace uo::assets
