// SPDX-License-Identifier: BSD-2-Clause
// Ported from ClassicUO.Assets (TexmapsLoader). TexTerr.def remapping is not ported yet.
#include "uo/assets/Texmaps.h"

#include "uo/assets/Color.h"
#include "uo/io/BinaryReader.h"

namespace uo::assets
{

Image Texmaps::texmap(std::uint32_t texId) const
{
    if (!has(texId))
        return {};
    return decode(_file->read(texId));
}

Image Texmaps::decode(std::span<const std::uint8_t> raw)
{
    const int size = raw.size() == 0x2000 ? 64 : 128;
    const std::size_t bytes = static_cast<std::size_t>(size) * size * 2;

    if (raw.size() < bytes)
        return {};

    Image img{size, size, std::vector<std::uint32_t>(static_cast<std::size_t>(size) * size, 0)};
    io::BinaryReader r(raw);

    for (auto& px : img.pixels)
        px = color16To32(r.readU16LE()) | kOpaque;

    return img;
}

}  // namespace uo::assets
