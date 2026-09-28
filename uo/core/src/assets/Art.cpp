// SPDX-License-Identifier: BSD-2-Clause
#include "uo/assets/Art.h"

#include "uo/assets/Color.h"
#include "uo/io/BinaryReader.h"

namespace uo::assets
{

Image Art::land(std::uint32_t id) const
{
    if (id >= kMaxLand)
        return {};
    return decodeLand(_file->read(id));
}

Image Art::statik(std::uint32_t graphic) const
{
    if (graphic >= kMaxStatic)
        return {};
    return decodeStatic(_file->read(graphic + kMaxLand));
}

Image Art::decodeLand(std::span<const std::uint8_t> raw)
{
    if (raw.size() < 2024)
        return {};

    Image img{44, 44, std::vector<std::uint32_t>(44 * 44, 0)};
    io::BinaryReader r(raw);

    // Top half widens by two pixels a row, bottom half narrows again.
    for (int i = 0; i < 22; ++i)
    {
        int start = 22 - (i + 1);
        int pos   = i * 44 + start;
        int end   = start + ((i + 1) << 1);
        for (int j = start; j < end; ++j)
            img.pixels[pos++] = color16To32(r.readU16LE()) | kOpaque;
    }
    for (int i = 0; i < 22; ++i)
    {
        int pos = (i + 22) * 44 + i;
        int end = i + ((22 - i) << 1);
        for (int j = i; j < end; ++j)
            img.pixels[pos++] = color16To32(r.readU16LE()) | kOpaque;
    }
    return img;
}

Image Art::decodeStatic(std::span<const std::uint8_t> raw)
{
    if (raw.size() < 8)
        return {};
    io::BinaryReader r(raw);
    r.readU32LE();  // flags
    int w = r.readI16LE();
    int h = r.readI16LE();
    return decodeRuns(raw.subspan(8), w, h);
}

Image Art::decodeRuns(std::span<const std::uint8_t> rows, int width, int height)
{
    if (width <= 0 || height <= 0 || width > 2048 || height > 2048 ||
        rows.size() < static_cast<std::size_t>(height) * 2)
        return {};

    Image img{width, height, std::vector<std::uint32_t>(static_cast<std::size_t>(width) * height, 0)};

    // Row table: one u16 per row, an offset (in u16 units) from the end of the table.
    io::BinaryReader table(rows);
    const std::size_t dataStart = static_cast<std::size_t>(height) * 2;
    io::BinaryReader r(rows);

    int x = 0, y = 0;
    r.seek(dataStart + static_cast<std::size_t>(table.readU16LE()) * 2);

    while (y < height && !r.overflowed())
    {
        std::uint16_t xoffs = r.readU16LE();
        std::uint16_t run   = r.readU16LE();

        if (xoffs + run >= 2048)
            break;

        if (xoffs + run != 0)
        {
            x += xoffs;
            if (x + run > width)
                break;  // file disagrees with its own header
            std::size_t pos = static_cast<std::size_t>(y) * width + x;
            for (int j = 0; j < run; ++j, ++pos)
            {
                std::uint16_t v = r.readU16LE();
                if (v != 0)
                    img.pixels[pos] = color16To32(v) | kOpaque;
            }
            x += run;
        }
        else
        {
            x = 0;
            ++y;
            if (y < height)
            {
                table.seek(static_cast<std::size_t>(y) * 2);
                r.seek(dataStart + static_cast<std::size_t>(table.readU16LE()) * 2);
            }
        }
    }
    return img;
}

}  // namespace uo::assets
