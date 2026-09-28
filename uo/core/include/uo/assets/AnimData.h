// SPDX-License-Identifier: BSD-2-Clause
// Part of the AxmolUO client core. Ported from ClassicUO.Assets (AnimDataLoader).
#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace uo::assets
{

// animdata.mul: static-art animations (fires, water wheels...). Groups of { u32 header;
// 8 x 68-byte entries }, one entry per graphic. Frame values are offsets added to the graphic.
struct AnimDataEntry
{
    std::array<std::int8_t, 64> frames{};
    std::uint8_t unknown       = 0;
    std::uint8_t frameCount    = 0;
    std::uint8_t frameInterval = 0;
    std::uint8_t frameStart    = 0;
};

class AnimData
{
public:
    static constexpr std::size_t kEntryBytes = 68;

    bool load(const std::string& path);
    void loadFromBytes(std::span<const std::uint8_t> bytes);

    // Entry for a static graphic, or nullptr past the end of the file.
    const AnimDataEntry* get(std::uint32_t graphic) const
    {
        return graphic < _entries.size() ? &_entries[graphic] : nullptr;
    }
    std::size_t count() const { return _entries.size(); }

private:
    std::vector<AnimDataEntry> _entries;
};

}  // namespace uo::assets
