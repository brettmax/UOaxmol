// SPDX-License-Identifier: BSD-2-Clause
#include "uo/assets/Gumps.h"

#include "uo/assets/Color.h"
#include "uo/io/BinaryReader.h"

#include <algorithm>

namespace uo::assets
{

Image Gumps::get(std::uint32_t id, std::uint16_t hue) const
{
    const io::FileIndex* e = _file->entry(id);
    if (!e)
        return {};
    if (e->compression == io::CompressionType::None)
    {
        if (e->width <= 0 || e->height <= 0)
            return {};
        return decode(_file->raw(*e), e->width, e->height, _hues, hue);
    }

    // Compressed UOP gumps carry { u32 width; u32 height } ahead of the rows.
    std::vector<std::uint8_t> data;
    bool bwt = false;
    if (!_file->readDecompressed(*e, data, &bwt) || bwt)
        return {};  // TODO(uo): BWT-wrapped gumps once the cliloc BWT decoder lands
    io::BinaryReader r(data);
    int w = static_cast<int>(r.readU32LE());
    int h = static_cast<int>(r.readU32LE());
    return decode(std::span<const std::uint8_t>(data).subspan(8), w, h, _hues, hue);
}

Image Gumps::decode(std::span<const std::uint8_t> raw, int width, int height, const Hues* hues, std::uint16_t hue)
{
    if (width <= 0 || height <= 0 || raw.size() < static_cast<std::size_t>(height) * 4)
        return {};

    Image img{width, height, std::vector<std::uint32_t>(static_cast<std::size_t>(width) * height, 0)};
    io::BinaryReader r(raw);
    const std::size_t halfLen = raw.size() >> 2;

    std::vector<std::int32_t> rows(height);
    for (auto& v : rows)
        v = r.readI32LE();

    for (int y = 0; y < height; ++y)
    {
        r.seek(static_cast<std::size_t>(rows[y]) << 2);
        std::size_t pixel = static_cast<std::size_t>(y) * width;
        std::size_t rowEnd = pixel + width;
        std::int64_t pairs = (y < height - 1) ? rows[y + 1] - rows[y] : static_cast<std::int64_t>(halfLen) - rows[y];

        for (std::int64_t i = 0; i < pairs && !r.overflowed(); ++i)
        {
            std::uint16_t value = r.readU16LE();
            std::uint16_t run   = r.readU16LE();

            if (hue != 0 && value != 0 && hues)
                value = hues->applyHue16(value, hue);

            std::uint32_t rgba = value ? (color16To32(value) | kOpaque) : 0;
            std::size_t n      = std::min<std::size_t>(run, rowEnd > pixel ? rowEnd - pixel : 0);
            std::fill_n(img.pixels.begin() + pixel, n, rgba);
            pixel += n;
            if (n < run)
                break;
        }
    }
    return img;
}

}  // namespace uo::assets
