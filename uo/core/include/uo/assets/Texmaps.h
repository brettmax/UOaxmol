// SPDX-License-Identifier: BSD-2-Clause
// Part of the AxmolUO client core. Ported from ClassicUO.Assets (TexmapsLoader).
#pragma once

#include "uo/assets/Image.h"
#include "uo/io/UOFile.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>

namespace uo::assets
{

// texmaps.mul + texidx.mul: square ground textures that stretched land tiles are drawn with,
// addressed by the land tile's tiledata texId. 0x2000-byte entries are 64x64, anything else
// 128x128, all 16-bit opaque colours.
class Texmaps
{
public:
    explicit Texmaps(std::unique_ptr<io::UOFile> file) : _file(std::move(file)) {}

    bool has(std::uint32_t texId) const
    {
        const io::FileIndex* e = _file ? _file->entry(texId) : nullptr;
        return e && e->valid();
    }

    Image texmap(std::uint32_t texId) const;

    static int sizeFor(std::size_t length) { return length == 0x2000 ? 64 : 128; }

    static Image decode(std::span<const std::uint8_t> raw);

private:
    std::unique_ptr<io::UOFile> _file;
};

}  // namespace uo::assets
