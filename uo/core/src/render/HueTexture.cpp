// SPDX-License-Identifier: BSD-2-Clause
#include "uo/render/HueTexture.h"

#include <algorithm>

namespace uo::render
{

std::vector<std::uint32_t> packHueTexture(std::span<const std::uint32_t> ramps)
{
    std::vector<std::uint32_t> pixels(static_cast<std::size_t>(kHueTextureWidth) * kHueTextureHeight, 0);
    const std::size_t count = std::min(ramps.size(), pixels.size());
    std::copy_n(ramps.begin(), count, pixels.begin());
    return pixels;
}

}  // namespace uo::render
