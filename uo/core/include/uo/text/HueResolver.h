// SPDX-License-Identifier: BSD-2-Clause
// Ported from ClassicUO (HuesHelper.cs, HuesLoader.cs).

#pragma once

#include "uo/assets/Color.h"
#include "uo/assets/Hues.h"

#include <cstdint>

namespace uo::text
{

using assets::color16To32;

inline uint32_t rgbaToArgb(uint32_t rgba)
{
    return (rgba >> 8) | (rgba << 24);
}

// The hue lookups the font renderer needs, with the exact HuesLoader semantics. The
// base class behaves like a client with no hues loaded; HuesResolver reads hues.mul.
class HueResolver
{
public:
    virtual ~HueResolver() = default;

    // Number of hues (8 per hues.mul group). Hue ids are 1-based; 0 means "no hue".
    virtual int hueCount() const { return 0; }

    // Color-table entry `index` (0..31) of 1-based `hue`. Only called when
    // 0 < hue < hueCount().
    virtual uint16_t colorTableEntry(uint16_t hue, int index) const
    {
        (void)hue;
        (void)index;
        return 0;
    }

    // HuesLoader.GetColor
    uint32_t color(uint16_t c, uint16_t hue) const
    {
        if (hue != 0 && hue < hueCount())
            return color16To32(colorTableEntry(hue, (c >> 10) & 0x1F));

        return hue != 0 ? color16To32(hue) : color16To32(c);
    }

    // HuesLoader.GetPartialHueColor: only grey pixels take the hue.
    uint32_t partialHueColor(uint16_t c, uint16_t hue) const
    {
        uint32_t cl = color16To32(c);

        if (hue != 0 && hue < hueCount())
        {
            const uint8_t r = cl & 0xFF;
            const uint8_t g = (cl >> 8) & 0xFF;
            const uint8_t b = (cl >> 16) & 0xFF;

            if (r == g && r == b)
                cl = color16To32(colorTableEntry(hue, (c >> 10) & 0x1F));
        }

        return cl;
    }

    // HuesLoader.GetPolygoneColor: entry `cell` of the hue's table, used for unicode text.
    uint32_t polygoneColor(uint16_t cell, uint16_t hue) const
    {
        if (hue != 0 && hue < hueCount())
            return color16To32(colorTableEntry(hue, cell & 0x1F));

        return 0xFF010101;
    }
};

// HueResolver over uo::assets::Hues.
class HuesResolver final : public HueResolver
{
public:
    explicit HuesResolver(const assets::Hues& hues) : _hues(hues) {}

    int hueCount() const override { return static_cast<int>(_hues.count()); }

    uint16_t colorTableEntry(uint16_t hue, int index) const override
    {
        const assets::HueEntry* e = _hues.entry(hue);
        return e ? e->colors[index & 0x1F] : 0;
    }

private:
    const assets::Hues& _hues;
};

}  // namespace uo::text
