// SPDX-License-Identifier: BSD-2-Clause
// Part of the AxmolUO client core. Ported from ClassicUO.Assets (HuesLoader).
#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace uo::assets
{

// hues.mul: groups of { u32 header; 8 x HueEntry }. A hue id is 1-based: hue h lives in
// group (h - 1) / 8, entry (h - 1) % 8. Hue 0 means "unhued".
struct HueEntry
{
    std::array<std::uint16_t, 32> colors{};
    std::uint16_t tableStart = 0;
    std::uint16_t tableEnd   = 0;
    std::string name;
};

class Hues
{
public:
    bool load(const std::string& huesPath, const std::string& radarcolPath);
    // Loads from memory, for tests and for data already pulled out of an archive.
    void loadFromBytes(const std::uint8_t* hues, std::size_t huesSize, const std::uint8_t* radar, std::size_t radarSize);

    std::size_t count() const { return _entries.size(); }
    const HueEntry* entry(std::uint16_t hue) const;

    // Recolour a 15-bit pixel through hue `hue` using its red channel as the ramp index,
    // the way the classic client tints items and gumps.
    std::uint16_t applyHue16(std::uint16_t pixel, std::uint16_t hue) const;
    std::uint32_t applyHue(std::uint16_t pixel, std::uint16_t hue) const;
    // Partial hues only recolour grey pixels (R == G == B) and leave the rest untouched.
    std::uint32_t applyPartialHue(std::uint16_t pixel, std::uint16_t hue) const;

    std::uint16_t radarColor(std::size_t index) const
    {
        return index < _radar.size() ? _radar[index] : 0;
    }

    // count() * 32 RGBA8 texels, one row of 32 per hue, for a hue lookup texture on the GPU.
    std::vector<std::uint32_t> buildHueTexture() const;

private:
    std::vector<HueEntry> _entries;
    std::vector<std::uint16_t> _radar;
};

}  // namespace uo::assets
