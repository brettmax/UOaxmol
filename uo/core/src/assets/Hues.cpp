// SPDX-License-Identifier: BSD-2-Clause
#include "uo/assets/Hues.h"

#include "uo/assets/Color.h"
#include "uo/io/BinaryReader.h"
#include "uo/io/MappedFile.h"

namespace uo::assets
{

namespace
{
constexpr std::size_t kEntrySize = 32 * 2 + 2 + 2 + 20;  // 88
constexpr std::size_t kGroupSize = 4 + 8 * kEntrySize;   // 708
}  // namespace

bool Hues::load(const std::string& huesPath, const std::string& radarcolPath)
{
    io::MappedFile hues(huesPath);
    if (!hues.isOpen())
        return false;
    io::MappedFile radar(radarcolPath);
    loadFromBytes(hues.data(), hues.size(), radar.data(), radar.size());
    return true;
}

void Hues::loadFromBytes(const std::uint8_t* hues, std::size_t huesSize, const std::uint8_t* radar,
                         std::size_t radarSize)
{
    _entries.clear();
    io::BinaryReader r({hues, huesSize});
    std::size_t groups = huesSize / kGroupSize;
    _entries.reserve(groups * 8);

    for (std::size_t g = 0; g < groups; ++g)
    {
        r.readU32LE();  // header
        for (int e = 0; e < 8; ++e)
        {
            HueEntry h;
            for (auto& c : h.colors)
                c = r.readU16LE();
            h.tableStart = r.readU16LE();
            h.tableEnd   = r.readU16LE();
            h.name       = r.readASCII(20);
            _entries.push_back(std::move(h));
        }
    }

    _radar.assign(radarSize / 2, 0);
    io::BinaryReader rr({radar, radarSize});
    for (auto& c : _radar)
        c = rr.readU16LE();
}

const HueEntry* Hues::entry(std::uint16_t hue) const
{
    if (hue == 0 || hue > _entries.size())
        return nullptr;
    return &_entries[hue - 1];
}

std::uint16_t Hues::applyHue16(std::uint16_t pixel, std::uint16_t hue) const
{
    const HueEntry* e = entry(hue);
    return e ? e->colors[(pixel >> 10) & 0x1F] : pixel;
}

std::uint32_t Hues::applyHue(std::uint16_t pixel, std::uint16_t hue) const
{
    return color16To32(applyHue16(pixel, hue));
}

std::uint32_t Hues::applyPartialHue(std::uint16_t pixel, std::uint16_t hue) const
{
    std::uint32_t cl  = color16To32(pixel);
    const HueEntry* e = entry(hue);
    if (!e)
        return cl;
    std::uint8_t r = cl & 0xFF, g = (cl >> 8) & 0xFF, b = (cl >> 16) & 0xFF;
    if (r == g && r == b)
        return color16To32(e->colors[(pixel >> 10) & 0x1F]);
    return cl;
}

std::vector<std::uint32_t> Hues::buildHueTexture() const
{
    std::vector<std::uint32_t> out;
    out.reserve(_entries.size() * 32);
    for (const auto& e : _entries)
        for (auto c : e.colors)
            out.push_back(color16To32(c) | kOpaque);
    return out;
}

}  // namespace uo::assets
