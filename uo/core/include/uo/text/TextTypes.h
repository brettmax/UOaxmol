// SPDX-License-Identifier: BSD-2-Clause
// Ported from ClassicUO (FontsLoader.cs, RenderedText.cs).

#pragma once

#include <cstdint>
#include <vector>

namespace uo::text
{

// Style flags accepted by the font renderer. Values match the original client's
// UOFONT_* constants so they can be passed straight through from gump data.
enum FontStyle : uint16_t
{
    FontStyleNone        = 0x0000,
    FontStyleSolid       = 0x0001,
    FontStyleItalic      = 0x0002,
    FontStyleIndention   = 0x0004,
    FontStyleBlackBorder = 0x0008,
    FontStyleUnderline   = 0x0010,
    FontStyleFixed       = 0x0020,
    FontStyleCropped     = 0x0040,
    FontStyleBQ          = 0x0080,
    FontStyleExtraHeight = 0x0100,
    FontStyleCropTexture = 0x0200,
    FontStyleFixedHeight = 0x0400,
};

enum class TextAlign : uint8_t
{
    Left = 0,
    Center,
    Right,
};

// One laid-out character. Color is 0xFFFFFFFF when the line's base hue applies.
struct GlyphRun
{
    uint32_t color  = 0;
    uint16_t flags  = 0;
    uint8_t font    = 0;
    char16_t item   = 0;
    uint16_t linkId = 0;
};

// One wrapped line of text, the equivalent of MultilinesFontInfo.
struct TextLine
{
    TextAlign align     = TextAlign::Left;
    int charCount       = 0;
    int charStart       = 0;
    int indentionOffset = 0;
    int maxHeight       = 0;
    int width           = 0;
    std::vector<GlyphRun> data;
};

using TextLayout = std::vector<TextLine>;

// A rendered string. Pixels are 32-bit values laid out as 0xAABBGGRR, i.e. RGBA8
// bytes in memory on little-endian hosts, ready for an RGBA8888 texture upload.
struct TextBitmap
{
    std::vector<uint32_t> pixels;
    int width     = 0;
    int height    = 0;
    int lineCount = 0;

    bool empty() const { return pixels.empty() || width <= 0 || height <= 0; }
};

}  // namespace uo::text
