// SPDX-License-Identifier: BSD-2-Clause
// Deterministic synthetic font data for the text tests. The expected values in
// TextTests.cpp were produced by running the original ClassicUO FontsLoader and
// ClilocLoader over exactly these bytes, so a change here invalidates them.
#pragma once

#include "TestUtil.h"

#include <cstdint>
#include <string>

namespace uotest::textfixture
{

// Small LCG so the data is identical on every platform and standard library.
struct Lcg
{
    std::uint32_t state;
    explicit Lcg(std::uint32_t seed) : state(seed) {}
    std::uint32_t next()
    {
        state = state * 1664525u + 1013904223u;
        return state >> 8;
    }
    int range(int lo, int hi) { return lo + static_cast<int>(next() % static_cast<std::uint32_t>(hi - lo)); }
};

inline std::uint16_t pixel(Lcg& r)
{
    const int k = r.range(0, 10);
    if (k < 4)
        return 0;
    if (k < 7)
    {
        const int g = r.range(1, 32);
        return static_cast<std::uint16_t>((g << 10) | (g << 5) | g);
    }
    return static_cast<std::uint16_t>(r.range(1, 0x8000));
}

// fonts.mul with 10 fonts; font 4 and 6 exercise their special line metrics.
inline Bytes fontsMul()
{
    Lcg r(7);
    Bytes b;
    for (int font = 0; font < 10; ++font)
    {
        b.push_back(0);
        for (int g = 0; g < 224; ++g)
        {
            int w = r.range(0, 12) == 0 ? 0 : r.range(1, 11);
            int h = r.range(0, 12) == 0 ? 0 : r.range(1, 15);
            if (g == 0)
            {
                w = r.range(3, 7);
                h = r.range(4, 12);
            }
            b.push_back(static_cast<std::uint8_t>(w));
            b.push_back(static_cast<std::uint8_t>(h));
            b.push_back(0);
            for (int i = 0; i < w * h; ++i)
                le16(b, pixel(r));
        }
    }
    return b;
}

// unifont.mul covering U+0021..U+017F with a few gaps and empty glyphs, and a space
// glyph that has metrics but no pixels.
inline Bytes unifontMul()
{
    Lcg r(11);
    Bytes b(0x40000, 0);
    auto put32 = [&](std::size_t at, std::uint32_t v) {
        for (int i = 0; i < 4; ++i)
            b[at + i] = static_cast<std::uint8_t>(v >> (i * 8));
    };

    put32(0x20 * 4, static_cast<std::uint32_t>(b.size()));
    b.insert(b.end(), {2, 3, 1, 0});

    for (int c = 0x21; c < 0x180; ++c)
    {
        if (r.range(0, 15) == 0)
            continue;
        put32(static_cast<std::size_t>(c) * 4, static_cast<std::uint32_t>(b.size()));
        const int w = r.range(0, 20) == 0 ? 0 : r.range(1, 13);
        const int h = r.range(0, 20) == 0 ? 0 : r.range(1, 15);
        b.push_back(static_cast<std::uint8_t>(static_cast<std::int8_t>(r.range(-2, 3))));
        b.push_back(static_cast<std::uint8_t>(static_cast<std::int8_t>(r.range(-1, 7))));
        b.push_back(static_cast<std::uint8_t>(w));
        b.push_back(static_cast<std::uint8_t>(h));
        if (w > 0 && h > 0)
            for (int i = 0; i < ((w - 1) / 8 + 1) * h; ++i)
                b.push_back(static_cast<std::uint8_t>(r.next()));
    }
    return b;
}

// 12 hues of random color tables, as { i32 count; u16[32] per hue }.
inline Bytes hueTables()
{
    Lcg r(13);
    Bytes b;
    le32(b, 12);
    for (int i = 0; i < 12 * 32; ++i)
        le16(b, static_cast<std::uint16_t>(r.range(0, 0x8000)));
    return b;
}

// A plain (uncompressed) Cliloc.enu.
inline Bytes clilocEnu()
{
    Bytes b;
    le32(b, 2);
    le16(b, 1);
    auto add = [&](std::int32_t n, const std::string& s) {
        le32(b, static_cast<std::uint32_t>(n));
        b.push_back(0);
        le16(b, static_cast<std::uint16_t>(s.size()));
        b.insert(b.end(), s.begin(), s.end());
    };
    add(1000, "a sword");
    add(1001, "");
    add(1002, "h\xC3\xA9llo w\xC3\xB6rld");
    add(500000, "You see: ~1_NAME~ with ~2_ITEM~");
    add(500001, "~1_AMOUNT~ gold ~3_x~ and ~2~");
    add(500002, "Broken ~_bad~ tail");
    add(500003, "Unclosed ~1_NAME");
    add(500004, "~10_BIG~ is ~1_A~");
    add(500005, "the quick  brown\tfox");
    add(3000000, "base value");
    return b;
}

inline const char* clilocsTxt()
{
    return "# ours\n3100001\tBreed\n3100002    Send to kennel\n3000000 override ~1_A~ x\n  bad line\n42\n";
}

}  // namespace uotest::textfixture
