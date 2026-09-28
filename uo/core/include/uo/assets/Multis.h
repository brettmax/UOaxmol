// SPDX-License-Identifier: BSD-2-Clause
// Part of the AxmolUO client core. Ported from ClassicUO.Assets (MultiLoader).
#pragma once

#include <cstdint>
#include <span>
#include <vector>

namespace uo::assets
{

struct MultiComponent
{
    std::uint16_t graphic = 0;
    std::int16_t x        = 0;
    std::int16_t y        = 0;
    std::int16_t z        = 0;
    std::uint32_t flags   = 0;
    bool visible          = false;  // flags != 0 in multi.mul: invisible parts are hidden fixtures
};

// multi.mul + multi.idx: a flat list of 12-byte records { u16 graphic; i16 x, y, z; u32 flags },
// 16 bytes (4 more unknown) from 7.0.9.0 on.
struct Multis
{
    static constexpr std::size_t kRecordOld = 12;
    static constexpr std::size_t kRecordNew = 16;

    static std::vector<MultiComponent> decode(std::span<const std::uint8_t> raw, bool newFormat);
};

}  // namespace uo::assets
