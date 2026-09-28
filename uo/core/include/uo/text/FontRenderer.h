// SPDX-License-Identifier: BSD-2-Clause
// Ported from ClassicUO (FontsLoader.cs).

#pragma once

#include "uo/text/HueResolver.h"
#include "uo/text/TextTypes.h"

#include <array>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace uo::assets
{
class Installation;
}

namespace uo::text
{

// fonts.mul glyph: 16-bit ARGB1555 pixels, 0 is transparent.
struct AsciiGlyph
{
    uint8_t width  = 0;
    uint8_t height = 0;
    std::vector<uint16_t> pixels;
};

// unifont*.mul glyph: 1 bit per pixel, rows padded to whole bytes, MSB first.
struct UnicodeGlyph
{
    int8_t offsetX      = 0;
    int8_t offsetY      = 0;
    int8_t width        = 0;
    int8_t height       = 0;
    const uint8_t* data = nullptr;  // points into the font file image; null when missing
};

// Loads the classic bitmap fonts and turns strings into pixels, line for line the
// way the original client does (wrapping, cropping, styles, hue application), so
// text keeps its original look and metrics. HTML gump text is not handled here yet.
//
// Not thread-safe: unicode glyphs are decoded lazily on first use.
class FontRenderer
{
public:
    static constexpr int kMaxUnicodeFonts  = 20;
    static constexpr int kAsciiGlyphCount  = 224;
    static constexpr int kUnicodeSpaceWidth = 8;

    FontRenderer();
    ~FontRenderer();

    FontRenderer(const FontRenderer&)            = delete;
    FontRenderer& operator=(const FontRenderer&) = delete;

    // Loads fonts.mul and unifont.mul .. unifont19.mul (memory-mapped) from the
    // installation. Returns false when fonts.mul is missing or holds no fonts.
    bool load(const assets::Installation& installation);

    // Parses a fonts.mul image. Returns the number of fonts found.
    int loadAsciiFonts(std::span<const uint8_t> data);

    // Installs unifont<index>.mul. When font 1 is absent it falls back to font 0,
    // like the original client.
    void setUnicodeFont(int index, std::vector<uint8_t> data);
    bool setUnicodeFont(int index, const std::string& path);

    // Hue lookups for colored text. Not owned; must outlive the renderer. When unset,
    // text renders as if no hues were loaded.
    void setHueResolver(const HueResolver* hues) { _hues = hues ? hues : &_noHues; }

    // Font 5 and 8 are always fully hued; others only hue grey pixels unless this is set.
    void setUnusePartialHue(bool value) { _unusePartialHue = value; }

    // When generating unicode text with width 0, size the bitmap from the laid-out lines.
    void setRecalculateWidthByInfo(bool value) { _recalculateWidthByInfo = value; }

    int asciiFontCount() const { return static_cast<int>(_ascii.size()); }
    bool unicodeFontExists(uint8_t font) const;

    const AsciiGlyph& asciiGlyph(uint8_t font, char16_t c) const;
    const UnicodeGlyph& unicodeGlyph(uint8_t font, char16_t c) const;

    // --- ASCII (fonts.mul) ---
    int widthAscii(uint8_t font, std::u16string_view str) const;
    int charWidthAscii(uint8_t font, char16_t c) const;
    int widthExAscii(uint8_t font, std::u16string_view str, int maxWidth, TextAlign align, uint16_t flags) const;
    int heightAscii(uint8_t font, std::u16string_view str, int width, TextAlign align, uint16_t flags) const;
    std::u16string textByWidthAscii(uint8_t font, std::u16string_view str, int width, bool isCropped) const;
    TextLayout layoutAscii(uint8_t font, std::u16string_view str, TextAlign align, uint16_t flags, int width,
                           bool countReturns = false, bool countSpaces = false) const;
    TextBitmap generateAscii(uint8_t font, std::u16string_view str, uint16_t hue, int width, TextAlign align,
                             uint16_t flags, int height = 0) const;

    // --- Unicode (unifont*.mul) ---
    int widthUnicode(uint8_t font, std::u16string_view str) const;
    int charWidthUnicode(uint8_t font, char16_t c) const;
    int widthExUnicode(uint8_t font, std::u16string_view str, int maxWidth, TextAlign align, uint16_t flags) const;
    int heightUnicode(uint8_t font, std::u16string_view str, int width, TextAlign align, uint16_t flags) const;
    std::u16string textByWidthUnicode(uint8_t font, std::u16string_view str, int width, bool isCropped) const;
    TextLayout layoutUnicode(uint8_t font, std::u16string_view str, TextAlign align, uint16_t flags, int width,
                             bool countReturns = false, bool countSpaces = false) const;
    // `cell` picks the hue color-table entry used for the text color (30 by default).
    TextBitmap generateUnicode(uint8_t font, std::u16string_view str, uint16_t hue, uint8_t cell, int width,
                               TextAlign align, uint16_t flags, int height = 0) const;

    static int layoutHeight(const TextLayout& layout);

private:
    struct UnicodeFont;

    TextBitmap pixelsAscii(uint8_t font, std::u16string_view str, uint16_t hue, int width, TextAlign align,
                           uint16_t flags) const;
    TextBitmap pixelsUnicode(uint8_t font, std::u16string_view str, uint16_t hue, uint8_t cell, int width,
                             TextAlign align, uint16_t flags) const;
    int fontOffsetY(uint8_t font, uint8_t index) const;

    std::vector<std::array<AsciiGlyph, kAsciiGlyphCount>> _ascii;
    std::array<std::shared_ptr<UnicodeFont>, kMaxUnicodeFonts> _unicode;
    HueResolver _noHues;
    const HueResolver* _hues     = &_noHues;
    bool _unusePartialHue        = false;
    bool _recalculateWidthByInfo = false;
};

}  // namespace uo::text
