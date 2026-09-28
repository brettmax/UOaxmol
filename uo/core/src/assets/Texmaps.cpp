// SPDX-License-Identifier: BSD-2-Clause
#include "uo/assets/Texmaps.h"

#include "uo/assets/Color.h"
#include "uo/io/BinaryReader.h"

namespace uo::assets
{

Image Texmaps::decode(std::span<const std::uint8_t> raw)
{
    if (raw.empty())
        return {};
    int size = sizeFor(raw.size());
    if (raw.size() < static_cast<std::size_t>(size) * size * 2)
        return {};

    Image img{size, size, std::vector<std::uint32_t>(static_cast<std::size_t>(size) * size)};
    io::BinaryReader r(raw);
    for (auto& px : img.pixels)
        px = color16To32(r.readU16LE()) | kOpaque;
    return img;
}

}  // namespace uo::assets
