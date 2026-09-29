// SPDX-License-Identifier: BSD-2-Clause
// Part of the AxmolUO client core. Ported from ClassicUO.Assets (MultiLoader).
#pragma once

#include <cstdint>
#include <memory>
#include <span>
#include <unordered_map>
#include <vector>

namespace uo::io
{
class UOFile;
}

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

// Footprint of a multi relative to its origin (Item.LoadMulti's MultiInfo).
struct MultiExtent
{
    std::int16_t minX = 0;
    std::int16_t minY = 0;
    std::int16_t maxX = 0;
    std::int16_t maxY = 0;
};

// multi.mul / multi.idx, decoded on first use (ClassicUO MultiLoader.GetMultis). Entry `id` is
// the multi graphic an 0x1A/0xF3 packet carries with the 0x4000 bit cleared.
class MultiLoader
{
public:
    MultiLoader(std::unique_ptr<io::UOFile> file, bool newFormat);
    ~MultiLoader();

    // Components of multi `id`, in file order; empty when the entry is absent. The reference stays
    // valid for the loader's lifetime.
    const std::vector<MultiComponent>& components(std::uint16_t id) const;

    // Bounds over every component, visible or not; all zero for an absent entry.
    MultiExtent extent(std::uint16_t id) const;

    std::size_t count() const;

private:
    std::unique_ptr<io::UOFile> _file;
    bool _newFormat = false;
    mutable std::unordered_map<std::uint16_t, std::vector<MultiComponent>> _cache;
};

}  // namespace uo::assets
