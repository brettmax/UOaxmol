// SPDX-License-Identifier: BSD-2-Clause
// Part of the AxmolUO client core. Ported from ClassicUO.Assets (ArtLoader).
#pragma once

#include "uo/assets/Image.h"
#include "uo/io/UOFile.h"

#include <cstdint>
#include <memory>
#include <span>

namespace uo::assets
{

// art.mul / artLegacyMUL.uop. Ids below kMaxLand are 44x44 land diamonds; the rest are
// statics (item art), addressed here by their graphic id (0-based, without the 0x4000 offset).
class Art
{
public:
    static constexpr std::uint32_t kMaxLand   = 0x4000;
    static constexpr std::uint32_t kMaxStatic = 0x14000 - kMaxLand;

    explicit Art(std::unique_ptr<io::UOFile> file) : _file(std::move(file)) {}

    Image land(std::uint32_t id) const;
    Image statik(std::uint32_t graphic) const;

    bool hasLand(std::uint32_t id) const { return id < kMaxLand && _file->entry(id); }
    bool hasStatic(std::uint32_t graphic) const { return graphic < kMaxStatic && _file->entry(graphic + kMaxLand); }

    // Decoders, exposed for tests and for the offline converter.
    static Image decodeLand(std::span<const std::uint8_t> raw);          // 2024 bytes, 22 + 22 rows
    static Image decodeStatic(std::span<const std::uint8_t> raw);        // { u32 flags; u16 w; u16 h; rows }
    static Image decodeRuns(std::span<const std::uint8_t> rows, int width, int height);

private:
    std::unique_ptr<io::UOFile> _file;
};

}  // namespace uo::assets
