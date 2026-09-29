// SPDX-License-Identifier: BSD-2-Clause
// Ported from ClassicUO (FontsLoader.cs: GetCaretPosASCII, GetCaretPosUnicode).

#include "uo/text/FontRenderer.h"

#include <algorithm>
#include <climits>
#include <cstdlib>

namespace uo::text
{

namespace
{

int alignedStartX(TextAlign align, int width)
{
    switch (align)
    {
    case TextAlign::Center: return width >> 1;
    case TextAlign::Right: return width;
    default: return 0;
    }
}

// The shared body of GetCaretPos*: walks the laid-out lines the way the original does.
template <typename CharWidth>
std::pair<int, int> caretFromLayout(const TextLayout& lines, int pos, int width, CharWidth charWidth)
{
    int x = 0;
    int y = 0;

    for (size_t n = 0; n < lines.size(); ++n)
    {
        const TextLine& info = lines[n];

        switch (info.align)
        {
        case TextAlign::Center: x = std::max((width - info.width) >> 1, 0); break;
        case TextAlign::Right: x = width; break;
        default: x = 0; break;
        }

        const int len = info.charCount;

        if (info.charStart == pos)
            return {x, y};

        if (pos <= info.charStart + len && static_cast<int>(info.data.size()) >= len)
        {
            for (int i = 0; i < len; i++)
            {
                x += charWidth(info.data[static_cast<size_t>(i)].item);

                if (info.charStart + i + 1 == pos)
                    return {x, y};
            }
        }
        else
        {
            x = width;
        }

        if (n + 1 < lines.size())
            y += info.maxHeight;
    }

    return {x, y};
}

// One character's caret advance, the body of GetCaretPosUnicode's loop.
int unicodeCaretAdvance(const FontRenderer& fonts, uint8_t font, char16_t c)
{
    const UnicodeGlyph& g = fonts.unicodeGlyph(font, c);

    if (c != u'\r' && g.data)
        return static_cast<int8_t>(g.offsetX + g.width + 1);

    return c == u' ' ? FontRenderer::kUnicodeSpaceWidth : 0;
}

// Nearest caret index to (px, py) among the caret positions of every index.
template <typename CaretAt>
int nearestCaret(int length, int px, int py, CaretAt caretAt)
{
    std::vector<std::pair<int, int>> carets;
    carets.reserve(static_cast<size_t>(length) + 1);

    for (int i = 0; i <= length; ++i)
        carets.push_back(caretAt(i));

    // The line: the lowest caret row starting at or above py, else the top row.
    int lineY = INT_MIN;
    int topY  = INT_MAX;

    for (const auto& [cx, cy] : carets)
    {
        topY = std::min(topY, cy);

        if (cy <= py)
            lineY = std::max(lineY, cy);
    }

    if (lineY == INT_MIN)
        lineY = topY;

    int best     = 0;
    int bestDist = INT_MAX;

    for (int i = 0; i <= length; ++i)
    {
        const auto& [cx, cy] = carets[static_cast<size_t>(i)];

        if (cy != lineY)
            continue;

        const int dist = std::abs(cx - px);

        if (dist < bestDist)
        {
            best     = i;
            bestDist = dist;
        }
    }

    return best;
}

}  // namespace

std::pair<int, int> FontRenderer::caretPosAscii(uint8_t font, std::u16string_view str, int pos, int width,
                                                TextAlign align, uint16_t flags) const
{
    const int x = alignedStartX(align, width);

    if (font >= _ascii.size() || str.empty())
        return {x, 0};

    if (width == 0)
        width = widthAscii(font, str);

    const TextLayout lines = layoutAscii(font, str, align, flags, width);

    if (lines.empty())
        return {x, 0};

    return caretFromLayout(lines, pos, width, [&](char16_t c) { return static_cast<int>(asciiGlyph(font, c).width); });
}

std::pair<int, int> FontRenderer::caretPosUnicode(uint8_t font, std::u16string_view str, int pos, int width,
                                                  TextAlign align, uint16_t flags) const
{
    const int x = alignedStartX(align, width);

    if (!unicodeFontExists(font) || str.empty())
        return {x, 0};

    if (width == 0)
        width = widthUnicode(font, str);

    const TextLayout lines = layoutUnicode(font, str, align, flags, width);

    if (lines.empty())
        return {x, 0};

    return caretFromLayout(lines, pos, width, [&](char16_t c) { return unicodeCaretAdvance(*this, font, c); });
}

int FontRenderer::caretIndexAscii(uint8_t font, std::u16string_view str, int x, int y, int width, TextAlign align,
                                  uint16_t flags) const
{
    if (font >= _ascii.size() || str.empty())
        return 0;

    if (width == 0)
        width = widthAscii(font, str);

    // One layout serves every index; caretPosAscii would redo it per call.
    const TextLayout lines = layoutAscii(font, str, align, flags, width);

    if (lines.empty())
        return 0;

    const auto charWidth = [&](char16_t c) { return static_cast<int>(asciiGlyph(font, c).width); };
    return nearestCaret(static_cast<int>(str.size()), x, y,
                        [&](int pos) { return caretFromLayout(lines, pos, width, charWidth); });
}

int FontRenderer::caretIndexUnicode(uint8_t font, std::u16string_view str, int x, int y, int width,
                                    TextAlign align, uint16_t flags) const
{
    if (!unicodeFontExists(font) || str.empty())
        return 0;

    if (width == 0)
        width = widthUnicode(font, str);

    const TextLayout lines = layoutUnicode(font, str, align, flags, width);

    if (lines.empty())
        return 0;

    const auto charWidth = [&](char16_t c) { return unicodeCaretAdvance(*this, font, c); };
    return nearestCaret(static_cast<int>(str.size()), x, y,
                        [&](int pos) { return caretFromLayout(lines, pos, width, charWidth); });
}

}  // namespace uo::text
