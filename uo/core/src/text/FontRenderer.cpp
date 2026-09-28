// SPDX-License-Identifier: BSD-2-Clause
// Ported from ClassicUO (FontsLoader.cs). The layout and pixel loops deliberately keep
// the original's control flow, quirks included, so wrapping and metrics match.

#include "uo/text/FontRenderer.h"

#include "uo/assets/Installation.h"
#include "uo/io/MappedFile.h"


#include <algorithm>
#include <string>

namespace uo::text
{

namespace
{
constexpr uint8_t kNoPrintChars          = 32;
constexpr float kItalicFontKoefficient   = 3.3f;
constexpr int kOffsetCharTable[10]       = {2, 0, 2, 2, 0, 0, 2, 2, 0, 0};
constexpr int kOffsetSymbolTable[10]     = {1, 0, 1, 1, -1, 0, 1, 1, 0, 0};
const AsciiGlyph kEmptyAscii{};
const UnicodeGlyph kEmptyUnicode{};

// ASCII fonts cover 256 characters: the low byte selects the glyph.
int asciiIndex(char16_t c)
{
    const auto ch = static_cast<uint8_t>(c);
    return ch < kNoPrintChars ? 0 : ch - kNoPrintChars;
}

int32_t readI32(const uint8_t* p)
{
    return static_cast<int32_t>(static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
                                (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24));
}

void setDataCount(std::vector<GlyphRun>& data, int target)
{
    data.resize(static_cast<size_t>(std::max(target, 0)));
}

TextLine& newLine(TextLayout& lines, TextAlign align)
{
    lines.emplace_back();
    lines.back().align = align;
    return lines.back();
}

bool isBlackColor(uint32_t c)
{
    return (c & 0xFF) <= 8 && ((c >> 8) & 0xFF) <= 8 && ((c >> 16) & 0xFF) <= 8;
}
}  // namespace

struct FontRenderer::UnicodeFont
{
    io::MappedFile mapped;
    std::vector<uint8_t> owned;
    std::span<const uint8_t> file;
    mutable std::vector<UnicodeGlyph> glyphs;
    mutable std::vector<bool> loaded;

    const UnicodeGlyph& glyph(char16_t c) const
    {
        const size_t index = c;

        if (glyphs.empty())
        {
            glyphs.resize(0x10000);
            loaded.resize(0x10000);
        }

        if (!loaded[index])
        {
            loaded[index] = true;
            glyphs[index] = decode(index);
        }

        return glyphs[index];
    }

    UnicodeGlyph decode(size_t index) const
    {
        UnicodeGlyph g;

        if (index * 4 + 4 > file.size())
            return g;

        const int32_t lookup = readI32(file.data() + index * 4);

        if (lookup <= 0 || static_cast<size_t>(lookup) + 4 > file.size())
            return g;

        const uint8_t* p = file.data() + lookup;
        g.offsetX        = static_cast<int8_t>(p[0]);
        g.offsetY        = static_cast<int8_t>(p[1]);
        g.width          = static_cast<int8_t>(p[2]);
        g.height         = static_cast<int8_t>(p[3]);

        if (g.width > 0 && g.height > 0)
        {
            const size_t size = static_cast<size_t>(((g.width - 1) / 8) + 1) * g.height;

            if (static_cast<size_t>(lookup) + 4 + size <= file.size())
                g.data = p + 4;
        }

        return g;
    }
};

FontRenderer::FontRenderer()  = default;
FontRenderer::~FontRenderer() = default;

bool FontRenderer::load(const assets::Installation& installation)
{
    for (int i = 0; i < kMaxUnicodeFonts; ++i)
    {
        const std::string name = "unifont" + (i == 0 ? std::string() : std::to_string(i)) + ".mul";

        if (installation.exists(name))
            setUnicodeFont(i, installation.path(name));
    }

    io::MappedFile fonts(installation.path("fonts.mul"));

    if (!fonts.isOpen())
        return false;

    return loadAsciiFonts(fonts.bytes()) > 0;
}

int FontRenderer::loadAsciiFonts(std::span<const uint8_t> data)
{
    _ascii.clear();

    const size_t length = data.size();
    size_t pos          = 0;
    int count           = 0;

    // First pass counts complete fonts: a header byte then 224 glyphs.
    while (pos < length)
    {
        bool exit = false;
        ++pos;

        for (int i = 0; i < kAsciiGlyphCount; ++i)
        {
            if (pos + 3 >= length)
                break;

            const size_t w = data[pos];
            const size_t h = data[pos + 1];
            pos += 3;

            const size_t bytes = w * h * 2;

            if (pos + bytes > length)
            {
                exit = true;
                break;
            }

            pos += bytes;
        }

        if (exit)
            break;

        ++count;
    }

    if (count < 1)
        return 0;

    _ascii.resize(count);
    pos = 0;

    for (int i = 0; i < count; ++i)
    {
        ++pos;  // header

        for (int j = 0; j < kAsciiGlyphCount; ++j)
        {
            if (pos + 3 >= length)
                continue;

            AsciiGlyph& g = _ascii[i][j];
            g.width       = data[pos];
            g.height      = data[pos + 1];
            pos += 3;

            g.pixels.assign(static_cast<size_t>(g.width) * g.height, 0);
            const size_t available = std::min(g.pixels.size(), (length - pos) / 2);

            for (size_t k = 0; k < available; ++k)
                g.pixels[k] = static_cast<uint16_t>(data[pos + k * 2] | (data[pos + k * 2 + 1] << 8));

            pos += std::min(g.pixels.size() * 2, length - pos);
        }
    }

    return count;
}

void FontRenderer::setUnicodeFont(int index, std::vector<uint8_t> data)
{
    if (index < 0 || index >= kMaxUnicodeFonts)
        return;

    auto font   = std::make_shared<UnicodeFont>();
    font->owned = std::move(data);
    font->file  = font->owned;
    _unicode[index] = std::move(font);
}

bool FontRenderer::setUnicodeFont(int index, const std::string& path)
{
    if (index < 0 || index >= kMaxUnicodeFonts)
        return false;

    auto font = std::make_shared<UnicodeFont>();

    if (!font->mapped.open(path))
        return false;

    font->file      = font->mapped.bytes();
    _unicode[index] = std::move(font);
    return true;
}

bool FontRenderer::unicodeFontExists(uint8_t font) const
{
    if (font >= kMaxUnicodeFonts)
        return false;

    return _unicode[font] != nullptr || (font == 1 && _unicode[0] != nullptr);
}

const AsciiGlyph& FontRenderer::asciiGlyph(uint8_t font, char16_t c) const
{
    if (font >= _ascii.size())
        return kEmptyAscii;

    return _ascii[font][asciiIndex(c)];
}

const UnicodeGlyph& FontRenderer::unicodeGlyph(uint8_t font, char16_t c) const
{
    if (font >= kMaxUnicodeFonts)
        return kEmptyUnicode;

    const UnicodeFont* f = _unicode[font].get();

    if (!f && font == 1)
        f = _unicode[0].get();

    if (!f)
        return kEmptyUnicode;

    // A glyph without pixels reads as all zeros, offsets included, like the original's
    // shared null character. Layout depends on it: a missing space measures 1, not its offsets.
    const UnicodeGlyph& g = f->glyph(c);
    return g.data ? g : kEmptyUnicode;
}

// ---------------------------------------------------------------------------
// ASCII
// ---------------------------------------------------------------------------

int FontRenderer::widthAscii(uint8_t font, std::u16string_view str) const
{
    if (font >= _ascii.size() || str.empty())
        return 0;

    int length = 0;

    for (char16_t c : str)
        length += _ascii[font][asciiIndex(c)].width;

    return length;
}

int FontRenderer::charWidthAscii(uint8_t font, char16_t c) const
{
    if (font >= _ascii.size() || c == 0 || c == u'\r')
        return 0;

    if (c < kNoPrintChars)
        return _ascii[font][0].width;

    const int index = c - kNoPrintChars;

    return index < kAsciiGlyphCount ? _ascii[font][index].width : 0;
}

int FontRenderer::widthExAscii(uint8_t font, std::u16string_view str, int maxWidth, TextAlign align,
                               uint16_t flags) const
{
    if (font > _ascii.size() || str.empty())
        return 0;

    int width = 0;

    for (const auto& line : layoutAscii(font, str, align, flags, maxWidth))
        width = std::max(width, line.width);

    return width;
}

int FontRenderer::layoutHeight(const TextLayout& layout)
{
    int height = 0;

    for (const auto& line : layout)
        height += line.maxHeight;

    return height;
}

int FontRenderer::heightAscii(uint8_t font, std::u16string_view str, int width, TextAlign align,
                              uint16_t flags) const
{
    if (width == 0)
        width = widthAscii(font, str);

    return layoutHeight(layoutAscii(font, str, align, flags, width));
}

std::u16string FontRenderer::textByWidthAscii(uint8_t font, std::u16string_view str, int width, bool isCropped) const
{
    if (font >= _ascii.size() || str.empty())
        return {};

    std::u16string out;
    out.reserve(str.size() + 3);

    if (isCropped)
        width -= _ascii[font][u'.' - kNoPrintChars].width * 3;

    int length = 0;

    for (char16_t c : str)
    {
        length += _ascii[font][asciiIndex(c)].width;

        if (length > width)
            break;

        out.push_back(c);
    }

    if (isCropped)
        out += u"...";

    return out;
}

TextLayout FontRenderer::layoutAscii(uint8_t font, std::u16string_view str, TextAlign align, uint16_t flags,
                                     int width, bool countReturns, bool countSpaces) const
{
    TextLayout lines;

    if (font >= _ascii.size())
        return lines;

    newLine(lines, align);

    int indentionOffset = 0;
    const bool isFixed  = (flags & FontStyleFixed) != 0;
    const bool isCropped = (flags & FontStyleCropped) != 0;
    int charCount       = 0;
    int lastSpace       = 0;
    int readWidth       = 0;
    const int newlineval = countReturns ? 1 : 0;
    const int len       = static_cast<int>(str.size());

    for (int i = 0; i < len; i++)
    {
        char16_t si = str[i];

        if (si == u'\n' && (isFixed || isCropped))
            continue;

        TextLine* ptr = &lines.back();

        if (si == u' ')
        {
            lastSpace = i;
            ptr->width += readWidth;
            readWidth = 0;
            ptr->charCount += charCount;
            charCount = 0;
        }

        // Taken before any rewind below; the rewound character inherits these metrics,
        // as in the original.
        const AsciiGlyph& fcd = _ascii[font][asciiIndex(si)];
        int eval              = ptr->charStart;

        if (si == u'\n' || ptr->width + readWidth + fcd.width > width)
        {
            if (lastSpace == ptr->charStart && lastSpace == 0 && si != u'\n')
                ++eval;

            if (si == u'\n')
            {
                ptr->width += readWidth;
                ptr->charCount += charCount + newlineval;
                lastSpace = i;

                if (ptr->width == 0)
                    ptr->width = 1;

                if (ptr->maxHeight == 0)
                    ptr->maxHeight = 14;

                setDataCount(ptr->data, ptr->charCount - newlineval);

                ptr            = &newLine(lines, align);
                ptr->charStart = i + 1;
                readWidth      = 0;
                charCount      = 0;
                indentionOffset = 0;
                ptr->indentionOffset = 0;

                continue;
            }

            if (lastSpace + 1 == eval && !isFixed && !isCropped)
            {
                ptr->width += readWidth;
                ptr->charCount += charCount;

                if (ptr->width == 0)
                    ptr->width = 1;

                if (ptr->maxHeight == 0)
                    ptr->maxHeight = 14;

                ptr            = &newLine(lines, align);
                ptr->charStart = i;
                lastSpace      = i - 1;
                charCount      = 0;

                if (ptr->align == TextAlign::Left && (flags & FontStyleIndention) != 0)
                    indentionOffset = 14;

                ptr->indentionOffset = indentionOffset;
                readWidth            = indentionOffset;
            }
            else
            {
                if (isFixed)
                {
                    ptr->data.push_back({0xFFFFFFFF, flags, font, si, 0});
                    readWidth += fcd.width;

                    if (fcd.height > ptr->maxHeight)
                        ptr->maxHeight = fcd.height;

                    charCount++;
                    ptr->width += readWidth;
                    ptr->charCount += charCount;
                }
                else if (isCropped)
                {
                    // Keep what was accumulated so a cropped single word is not emptied.
                    ptr->width += readWidth;
                    ptr->charCount += charCount;
                }

                i  = lastSpace + 1;
                si = i < len ? str[i] : u'\0';

                if (ptr->width == 0)
                {
                    ptr->width = 1;
                }
                else if (countSpaces && si != u'\0' && lastSpace - eval == ptr->charCount)
                {
                    ptr->charCount++;
                }

                if (ptr->maxHeight == 0)
                    ptr->maxHeight = 14;

                charCount = 0;
                setDataCount(ptr->data, ptr->charCount);

                if (isFixed || isCropped)
                    break;

                ptr            = &newLine(lines, align);
                ptr->charStart = i;

                if (ptr->align == TextAlign::Left && (flags & FontStyleIndention) != 0)
                    indentionOffset = 14;

                ptr->indentionOffset = indentionOffset;
                readWidth            = indentionOffset;
            }
        }

        ptr->data.push_back({0xFFFFFFFF, flags, font, si, 0});
        readWidth += si == u'\r' ? 0 : fcd.width;

        if (fcd.height > ptr->maxHeight)
            ptr->maxHeight = fcd.height;

        charCount++;
    }

    TextLine& last = lines.back();
    last.width += readWidth;
    last.charCount += charCount;

    if (readWidth == 0 && len > 0 && (str[len - 1] == u'\n' || str[len - 1] == u'\r'))
    {
        last.width     = 1;
        last.maxHeight = 14;
    }

    if (font == 4)
    {
        for (auto& line : lines)
            line.maxHeight += line.width > 1 ? 2 : 6;
    }

    return lines;
}

int FontRenderer::fontOffsetY(uint8_t font, uint8_t index) const
{
    if (index == 0xB8)
        return 1;

    if (!(index >= 0x41 && index <= 0x5A) && !(index >= 0xC0 && index <= 0xDF) && index != 0xA8)
    {
        if (font < 10)
        {
            if (index >= 0x61 && index <= 0x7A)
                return kOffsetCharTable[font];

            return kOffsetSymbolTable[font];
        }

        return 2;
    }

    return 0;
}

TextBitmap FontRenderer::generateAscii(uint8_t font, std::u16string_view str, uint16_t hue, int width,
                                       TextAlign align, uint16_t flags, int height) const
{
    if (str.empty())
        return {};

    if ((flags & (FontStyleFixed | FontStyleCropped | FontStyleCropTexture)) != 0)
    {
        if (width == 0)
            return {};

        if (widthAscii(font, str) > width)
        {
            const bool cropped = (flags & FontStyleCropped) != 0;
            std::u16string newStr = textByWidthAscii(font, str, width, cropped);

            if ((flags & FontStyleCropTexture) != 0 && !newStr.empty())
            {
                int totalHeight = 0;

                while (totalHeight < height)
                {
                    totalHeight += heightAscii(font, newStr, width, align, flags);

                    if (str.size() > newStr.size())
                        newStr += textByWidthAscii(font, str.substr(newStr.size()), width, cropped);
                    else
                        break;
                }
            }

            return pixelsAscii(font, newStr, hue, width, align, flags);
        }
    }

    return pixelsAscii(font, str, hue, width, align, flags);
}

TextBitmap FontRenderer::pixelsAscii(uint8_t font, std::u16string_view str, uint16_t hue, int width,
                                     TextAlign align, uint16_t flags) const
{
    if (font >= _ascii.size() || str.empty())
        return {};

    if (width <= 0)
        width = widthAscii(font, str);

    if (width <= 0)
        return {};

    const TextLayout lines = layoutAscii(font, str, align, flags, width);

    if (lines.empty())
        return {};

    width += 4;
    const int height = layoutHeight(lines);

    if (height <= 0)
        return {};

    TextBitmap out;
    out.width  = width;
    out.height = height;
    out.pixels.assign(static_cast<size_t>(width) * height, 0);

    const bool isPartial   = font != 5 && font != 8 && !_unusePartialHue;
    const int font6OffsetY = font == 6 ? 7 : 0;
    int lineOffsY          = 0;

    for (const auto& line : lines)
    {
        out.lineCount++;
        int w = 0;

        switch (line.align)
        {
        case TextAlign::Center:
            w = std::max((width - line.width) >> 1, 0);
            break;
        case TextAlign::Right:
            w = width - 10 - line.width;
            if (w < 0)
                w = width;
            break;
        case TextAlign::Left:
            if ((flags & FontStyleIndention) != 0)
                w = line.indentionOffset;
            break;
        }

        for (const auto& item : line.data)
        {
            const auto index      = static_cast<uint8_t>(item.item);
            const int offsY       = fontOffsetY(font, index);
            const AsciiGlyph& fcd = _ascii[font][asciiIndex(item.item)];
            const int dw          = fcd.width;
            const int dh          = fcd.height;

            for (int y = 0; y < dh; y++)
            {
                const int testY = y + lineOffsY + offsY;

                if (testY >= height)
                    break;

                for (int x = 0; x < dw; x++)
                {
                    if (x + w >= width)
                        break;

                    const uint16_t pic = fcd.pixels[y * dw + x];

                    if (pic != 0)
                    {
                        const uint32_t pcl = isPartial ? _hues->partialHueColor(pic, hue) : _hues->color(pic, hue);
                        const int block    = testY * width + x + w;

                        if (block >= 0 && static_cast<size_t>(block) < out.pixels.size())
                            out.pixels[block] = pcl | 0xFF000000;
                    }
                }
            }

            w += dw;
        }

        lineOffsY += line.maxHeight - font6OffsetY;
    }

    return out;
}

// ---------------------------------------------------------------------------
// Unicode
// ---------------------------------------------------------------------------

int FontRenderer::widthUnicode(uint8_t font, std::u16string_view str) const
{
    if (!unicodeFontExists(font) || str.empty())
        return 0;

    int length    = 0;
    int maxLength = 0;

    for (char16_t c : str)
    {
        const UnicodeGlyph& g = unicodeGlyph(font, c);

        if (c != u'\r' && g.data)
            length += static_cast<int8_t>(g.offsetX + g.width + 1);
        else if (c == u' ')
            length += kUnicodeSpaceWidth;
        else if (c == u'\n')
        {
            maxLength = std::max(maxLength, length);
            length    = 0;
        }
    }

    return std::max(maxLength, length);
}

int FontRenderer::charWidthUnicode(uint8_t font, char16_t c) const
{
    if (!unicodeFontExists(font) || c == 0 || c == u'\r')
        return 0;

    const UnicodeGlyph& g = unicodeGlyph(font, c);

    if (g.data)
        return static_cast<int8_t>(g.offsetX + g.width + 1);

    return c == u' ' ? kUnicodeSpaceWidth : 0;
}

int FontRenderer::widthExUnicode(uint8_t font, std::u16string_view str, int maxWidth, TextAlign align,
                                 uint16_t flags) const
{
    if (!unicodeFontExists(font) || str.empty())
        return 0;

    int width = 0;

    for (const auto& line : layoutUnicode(font, str, align, flags, maxWidth))
        width = std::max(width, line.width);

    return width + 4;
}

int FontRenderer::heightUnicode(uint8_t font, std::u16string_view str, int width, TextAlign align,
                                uint16_t flags) const
{
    if (!unicodeFontExists(font) || str.empty())
        return 0;

    if (width <= 0)
        width = widthUnicode(font, str);

    return layoutHeight(layoutUnicode(font, str, align, flags, width));
}

std::u16string FontRenderer::textByWidthUnicode(uint8_t font, std::u16string_view str, int width,
                                                bool isCropped) const
{
    if (!unicodeFontExists(font) || str.empty())
        return {};

    std::u16string out;
    out.reserve(str.size() + 3);

    if (isCropped)
    {
        const UnicodeGlyph& dot = unicodeGlyph(font, u'.');

        if (dot.data)
            width -= dot.width * 3 + 3;
    }

    int length = 0;

    for (char16_t c : str)
    {
        const UnicodeGlyph& g = unicodeGlyph(font, c);
        int8_t charWidth      = 0;

        if (g.data)
            charWidth = static_cast<int8_t>(g.offsetX + g.width + 1);
        else if (c == u' ')
            charWidth = kUnicodeSpaceWidth;

        if (charWidth != 0)
        {
            length += charWidth;

            if (length > width)
                break;

            out.push_back(c);
        }
    }

    if (isCropped)
        out += u"...";

    return out;
}

TextLayout FontRenderer::layoutUnicode(uint8_t font, std::u16string_view str, TextAlign align, uint16_t flags,
                                       int width, bool countReturns, bool countSpaces) const
{
    TextLayout lines;

    if (!unicodeFontExists(font))
        return lines;

    newLine(lines, align);

    int indentionOffset   = 0;
    int charCount         = 0;
    int lastSpace         = 0;
    int readWidth         = 0;
    const int newlineval  = countReturns ? 1 : 0;
    const int extraHeight = (flags & FontStyleExtraHeight) != 0 ? 4 : 0;
    const bool isFixed    = (flags & FontStyleFixed) != 0;
    const bool isCropped  = (flags & FontStyleCropped) != 0;
    const TextAlign currentAlign = align;
    const uint16_t currentFlags  = flags;
    const uint8_t currentFont    = font;
    // Per-character colors only change under HTML, which this renderer does not parse.
    const uint32_t currentCharColor = 0xFFFFFFFF;
    const int len = static_cast<int>(str.size());

    for (int i = 0; i < len; i++)
    {
        char16_t si = str[i];

        if (si == u'\n' && (isFixed || isCropped))
            si = 0;

        const UnicodeGlyph& g = unicodeGlyph(font, si);

        if (!g.data && si != u' ' && si != u'\n' && si != u'\r')
            continue;

        // Taken before any rewind below; the rewound character inherits these metrics,
        // as in the original.
        const int charWidth  = g.offsetX + g.width + 1;
        const int charHeight = g.offsetY + g.height;
        TextLine* ptr        = &lines.back();

        if (si == u' ')
        {
            lastSpace = i;
            ptr->width += readWidth;
            readWidth = 0;
            ptr->charCount += charCount;
            charCount = 0;
        }

        int eval = ptr->charStart;

        if (ptr->width + readWidth + charWidth > width || si == u'\n')
        {
            if (lastSpace == ptr->charStart && lastSpace == 0 && si != u'\n')
                ++eval;

            if (si == u'\n')
            {
                ptr->width += readWidth;
                ptr->charCount += charCount + newlineval;
                lastSpace = i;

                if (ptr->width == 0)
                    ptr->width = 1;

                if (ptr->maxHeight == 0)
                    ptr->maxHeight = 14 + extraHeight;

                setDataCount(ptr->data, ptr->charCount - newlineval);

                ptr                  = &newLine(lines, currentAlign);
                ptr->charStart       = i + 1;
                readWidth            = 0;
                charCount            = 0;
                indentionOffset      = 0;
                ptr->indentionOffset = 0;

                continue;
            }

            if (lastSpace + 1 == eval && !isFixed && !isCropped)
            {
                ptr->width += readWidth;
                ptr->charCount += charCount;

                if (ptr->width == 0)
                    ptr->width = 1;

                if (ptr->maxHeight == 0)
                    ptr->maxHeight = 14 + extraHeight;

                ptr            = &newLine(lines, currentAlign);
                ptr->charStart = i;
                lastSpace      = i - 1;
                charCount      = 0;

                if (ptr->align == TextAlign::Left && (currentFlags & FontStyleIndention) != 0)
                    indentionOffset = 14;

                ptr->indentionOffset = indentionOffset;
                readWidth            = indentionOffset;
            }
            else
            {
                if (isFixed)
                {
                    ptr->data.push_back({currentCharColor, currentFlags, currentFont, si, 0});
                    readWidth += si == u'\r' ? 0 : charWidth;

                    if (charHeight > ptr->maxHeight)
                        ptr->maxHeight = charHeight + extraHeight;

                    charCount++;
                    ptr->width += readWidth;
                    ptr->charCount += charCount;
                }
                else if (isCropped)
                {
                    // Keep what was accumulated so a cropped single word is not emptied.
                    ptr->width += readWidth;
                    ptr->charCount += charCount;
                }

                i  = lastSpace + 1;
                si = i < len ? str[i] : u'\0';

                if (ptr->width == 0)
                {
                    ptr->width = 1;
                }
                else if (countSpaces && si != u'\0' && lastSpace - eval == ptr->charCount)
                {
                    ptr->charCount++;
                }

                if (ptr->maxHeight == 0)
                    ptr->maxHeight = 14 + extraHeight;

                charCount = 0;
                setDataCount(ptr->data, ptr->charCount);

                if (isFixed || isCropped)
                    break;

                ptr            = &newLine(lines, currentAlign);
                ptr->charStart = i;
                charCount      = 0;

                if (ptr->align == TextAlign::Left && (currentFlags & FontStyleIndention) != 0)
                    indentionOffset = 14;

                ptr->indentionOffset = indentionOffset;
                readWidth            = indentionOffset;
            }
        }

        ptr->data.push_back({currentCharColor, currentFlags, currentFont, si, 0});

        if (si == u' ')
        {
            readWidth += kUnicodeSpaceWidth;

            if (ptr->maxHeight <= 0)
                ptr->maxHeight = 5 + extraHeight;
        }
        else
        {
            readWidth += si == u'\r' ? 0 : charWidth;

            if (charHeight > ptr->maxHeight)
                ptr->maxHeight = charHeight + extraHeight;
        }

        charCount++;
    }

    TextLine& last = lines.back();
    last.width += readWidth;
    last.charCount += charCount;

    if (readWidth == 0 && len != 0)
    {
        switch (str[len - 1])
        {
        case u'\n':
            last.charCount += newlineval;
            [[fallthrough]];
        case u'\r':
            last.width     = 1;
            last.maxHeight = 14;
            break;
        default:
            break;
        }
    }

    return lines;
}

TextBitmap FontRenderer::generateUnicode(uint8_t font, std::u16string_view str, uint16_t hue, uint8_t cell,
                                         int width, TextAlign align, uint16_t flags, int height) const
{
    if (str.empty())
        return {};

    if ((flags & (FontStyleFixed | FontStyleCropped | FontStyleCropTexture)) != 0)
    {
        if (width == 0)
            return {};

        if (widthUnicode(font, str) > width)
        {
            const bool cropped    = (flags & FontStyleCropped) != 0;
            std::u16string newStr = textByWidthUnicode(font, str, width, cropped);

            if ((flags & FontStyleCropTexture) != 0 && !newStr.empty())
            {
                int totalHeight = 0;

                while (totalHeight < height)
                {
                    totalHeight += heightUnicode(font, newStr, width, align, flags);

                    // The original passes the already-consumed prefix here, which repeats
                    // the start of the text; the ASCII path takes the remainder, as here.
                    if (str.size() > newStr.size())
                        newStr += textByWidthUnicode(font, str.substr(newStr.size()), width, cropped);
                    else
                        break;
                }
            }

            return pixelsUnicode(font, newStr, hue, cell, width, align, flags);
        }
    }

    return pixelsUnicode(font, str, hue, cell, width, align, flags);
}

TextBitmap FontRenderer::pixelsUnicode(uint8_t font, std::u16string_view str, uint16_t hue, uint8_t cell,
                                       int width, TextAlign align, uint16_t flags) const
{
    if (!unicodeFontExists(font) || str.empty())
        return {};

    const int oldWidth = width;

    if (width == 0)
    {
        width = widthUnicode(font, str);

        if (width == 0)
            return {};
    }

    const TextLayout lines = layoutUnicode(font, str, align, flags, width);

    if (lines.empty())
        return {};

    if (oldWidth == 0 && _recalculateWidthByInfo)
    {
        width = 0;

        for (const auto& line : lines)
            width = std::max(width, line.width);
    }

    width += 4;
    int height = layoutHeight(lines);

    if (height == 0)
        return {};

    height += 4;

    TextBitmap out;
    out.width  = width;
    out.height = height;
    out.pixels.assign(static_cast<size_t>(width) * height, 0);
    uint32_t* pData = out.pixels.data();

    const uint32_t datacolor =
        hue == 0xFFFF ? 0xFEFFFFFF : rgbaToArgb((_hues->polygoneColor(cell, hue) << 8) | 0xFF);

    const bool isItalic      = (flags & FontStyleItalic) != 0;
    const bool isSolid       = (flags & FontStyleSolid) != 0;
    const bool isBlackBorder = (flags & FontStyleBlackBorder) != 0;
    const bool isUnderline   = (flags & FontStyleUnderline) != 0;
    constexpr uint32_t blackColor = 0xFF010101;
    int lineOffsY = 0;

    for (const auto& line : lines)
    {
        out.lineCount++;
        int w = 0;

        switch (line.align)
        {
        case TextAlign::Center:
            w += (width - 8) / 2 - line.width / 2;
            if (w < 0)
                w = 0;
            break;
        case TextAlign::Right:
            w += width - 10 - line.width;
            if (w < 0)
                w = 0;
            break;
        case TextAlign::Left:
            if ((flags & FontStyleIndention) != 0)
                w += line.indentionOffset;
            break;
        }

        bool stopLine = false;

        for (const auto& dataPtr : line.data)
        {
            if (stopLine)
                break;

            const char16_t si     = dataPtr.item;
            const UnicodeGlyph& g = unicodeGlyph(dataPtr.font, si);

            if (!g.data && si != u' ')
                continue;

            int offsX = 0;
            int offsY = 0;
            int dw    = 0;
            int dh    = 0;

            if (si == u' ')
            {
                dw = kUnicodeSpaceWidth;
            }
            else
            {
                offsX = g.offsetX + 1;
                offsY = g.offsetY;
                dw    = g.width;
                dh    = g.height;
            }

            const int tmpW           = w;
            const uint32_t charcolor = datacolor;
            const bool isBlackPixel  = isBlackColor(charcolor);

            if (si != u' ')
            {
                const int scanlineCount = ((dw - 1) >> 3) + 1;
                int scanLineOff         = 0;

                for (int y = 0; y < dh; y++, scanLineOff += scanlineCount)
                {
                    int testY = offsY + lineOffsY + y;

                    if (testY < 0)
                        testY = 0;

                    if (testY >= height)
                        break;

                    const int italicOffset = isItalic ? static_cast<int>((dh - y) / kItalicFontKoefficient) : 0;
                    const int testX        = w + offsX + italicOffset + (isSolid ? 1 : 0);

                    for (int c = 0; c < scanlineCount; c++)
                    {
                        const int coff = c << 3;

                        for (int j = 0; j < 8; j++)
                        {
                            const int x = coff + j;

                            if (x >= dw)
                                break;

                            const int nowX = testX + x;

                            if (nowX >= width)
                                break;

                            const int block = testY * width + nowX;

                            // A negative glyph offset can push the first column left of
                            // the bitmap; the original faulted there, so skip it.
                            if ((g.data[scanLineOff + c] & (1 << (7 - j))) != 0 && block >= 0)
                                pData[block] = charcolor;
                        }
                    }
                }

                if (isSolid)
                {
                    uint32_t solidColor = blackColor;

                    if (solidColor == charcolor)
                        solidColor++;

                    const int minXOk = w + offsX > 0 ? -1 : 0;
                    const int maxXOk = (w + offsX + dw < width ? 1 : 0) + dw;

                    for (int cy = 0; cy < dh; cy++)
                    {
                        int testY = offsY + lineOffsY + cy;

                        if (testY >= height)
                            break;

                        if (testY < 0)
                            testY = 0;

                        const int italicOffset =
                            isItalic && cy < dh ? static_cast<int>((dh - cy) / kItalicFontKoefficient) : 0;

                        for (int cx = minXOk; cx < maxXOk; cx++)
                        {
                            const int testX = cx + w + offsX + italicOffset;

                            if (testX >= width)
                                break;

                            const int block = testY * width + testX;

                            if (block < 0)
                                continue;

                            if (pData[block] == 0 && pData[block] != solidColor)
                            {
                                int endX = cx < dw ? 2 : 1;

                                if (endX == 2 && testX + 1 >= width)
                                    endX--;

                                for (int x = 0; x < endX; x++)
                                {
                                    const int testBlock = testY * width + testX + x;

                                    if (testBlock >= 0 && pData[testBlock] != 0 && pData[testBlock] != solidColor)
                                    {
                                        pData[block] = solidColor;
                                        break;
                                    }
                                }
                            }
                        }
                    }

                    for (int cy = 0; cy < dh; cy++)
                    {
                        int testY = offsY + lineOffsY + cy;

                        if (testY >= height)
                            break;

                        if (testY < 0)
                            testY = 0;

                        const int italicOffset = isItalic ? static_cast<int>((dh - cy) / kItalicFontKoefficient) : 0;

                        for (int cx = 0; cx < dw; cx++)
                        {
                            const int testX = cx + w + offsX + italicOffset;

                            if (testX >= width)
                                break;

                            const int block = testY * width + testX;

                            if (block >= 0 && pData[block] == solidColor)
                                pData[block] = charcolor;
                        }
                    }
                }

                if (isBlackBorder && !isBlackPixel)
                {
                    const int minXOk = w + offsX > 0 ? -1 : 0;
                    const int minYOk = offsY + lineOffsY > 0 ? -1 : 0;
                    const int maxXOk = (w + offsX + dw < width ? 1 : 0) + dw;
                    const int maxYOk = (offsY + lineOffsY + dh < height ? 1 : 0) + dh;
                    const int total  = width * height;

                    for (int cy = minYOk; cy < maxYOk; cy++)
                    {
                        int testY = offsY + lineOffsY + cy;

                        if (testY < 0)
                            testY = 0;

                        if (testY >= height)
                            break;

                        const int italicOffset = isItalic && cy >= 0 && cy < dh
                                                     ? static_cast<int>((dh - cy) / kItalicFontKoefficient)
                                                     : 0;

                        for (int cx = minXOk; cx < maxXOk; cx++)
                        {
                            const int testX = cx + w + offsX + italicOffset;

                            if (testX >= width)
                                break;

                            const int block = testY * width + testX;

                            if (block < 0)
                                continue;

                            if (pData[block] == 0 && pData[block] != blackColor)
                            {
                                const int startX = cx > 0 ? -1 : 0;
                                const int startY = cy > 0 ? -1 : 0;
                                int endX         = cx < dw - 1 ? 2 : 1;
                                const int endY   = cy < dh - 1 ? 2 : 1;

                                if (endX == 2 && testX + 1 >= width)
                                    endX--;

                                bool passed = false;

                                for (int x = startX; x < endX && !passed; x++)
                                {
                                    const int nowX = testX + x;

                                    for (int y = startY; y < endY; y++)
                                    {
                                        const int testBlock = (testY + y) * width + nowX;

                                        if (testBlock < 0)
                                            continue;

                                        if (testBlock < total && pData[testBlock] != 0 &&
                                            pData[testBlock] != blackColor)
                                        {
                                            pData[block] = blackColor;
                                            passed       = true;
                                            break;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                w += dw + offsX + (isSolid ? 1 : 0);
            }
            else
            {
                w += kUnicodeSpaceWidth;
            }

            if (isUnderline)
            {
                const int minXOk      = tmpW + offsX > 0 ? -1 : 0;
                const int maxXOk      = w + offsX + dw < width ? 1 : 0;
                const UnicodeGlyph& a = unicodeGlyph(font, u'a');
                int testY             = lineOffsY + a.offsetY + a.height;

                if (testY >= height)
                {
                    stopLine = true;
                    continue;
                }

                if (testY < 0)
                    testY = 0;

                for (int cx = minXOk; cx < dw + maxXOk; cx++)
                {
                    const int testX = cx + tmpW + offsX + (isSolid ? 1 : 0);

                    if (testX >= width)
                        break;

                    const int block = testY * width + testX;

                    if (block >= 0)
                        pData[block] = charcolor;
                }
            }
        }

        lineOffsY += line.maxHeight;
    }

    return out;
}

}  // namespace uo::text
