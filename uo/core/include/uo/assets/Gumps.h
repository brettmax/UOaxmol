// SPDX-License-Identifier: BSD-2-Clause
// Part of the AxmolUO client core. Ported from ClassicUO.Assets (GumpsLoader).
#pragma once

#include "uo/assets/Hues.h"
#include "uo/assets/Image.h"
#include "uo/io/UOFile.h"

#include <memory>
#include <span>

namespace uo::assets
{

// gumpart.mul + gumpidx.mul, or gumpartLegacyMUL.uop. Width/height come from the index.
class Gumps
{
public:
    explicit Gumps(std::unique_ptr<io::UOFile> file, const Hues* hues = nullptr)
        : _file(std::move(file)), _hues(hues)
    {}

    Image get(std::uint32_t id, std::uint16_t hue = 0) const;
    bool has(std::uint32_t id) const { return _file->entry(id) != nullptr; }

    // Rows: a table of one i32 per row (offset in 4-byte units from the table start), then
    // (u16 colour, u16 run) pairs. Colour 0 is transparent.
    static Image decode(std::span<const std::uint8_t> raw, int width, int height, const Hues* hues = nullptr,
                        std::uint16_t hue = 0);

private:
    std::unique_ptr<io::UOFile> _file;
    const Hues* _hues;
};

}  // namespace uo::assets
