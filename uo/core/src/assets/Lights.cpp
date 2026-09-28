// SPDX-License-Identifier: BSD-2-Clause
#include "uo/assets/Lights.h"

#include "uo/assets/Color.h"

namespace uo::assets
{

Image Lights::decode(std::span<const std::uint8_t> raw, int width, int height)
{
    if (width <= 0 || height <= 0 || raw.size() < static_cast<std::size_t>(width) * height)
        return {};

    Image img{width, height, std::vector<std::uint32_t>(static_cast<std::size_t>(width) * height, 0)};
    for (std::size_t i = 0; i < img.pixels.size(); ++i)
    {
        std::uint32_t v = raw[i];
        if (v > 0x1F)
            v = ~v & 0x1F;
        if (v != 0)
            img.pixels[i] = (v << 19) | (v << 11) | (v << 3) | kOpaque;
    }
    return img;
}

}  // namespace uo::assets
