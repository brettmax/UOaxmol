// SPDX-License-Identifier: BSD-2-Clause
// Ported from ClassicUO (FontsLoader.cs).

#pragma once

#include "uo/text/HueResolver.h"
#include "uo/text/TextTypes.h"

#include <array>
#include <cstdint>
#include <list>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
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
// text keeps its original look and metrics.
//
// HTML (gump htmlgump/xmfhtmlgump text) is a mode, as in the original: while setUseHtml(true)
// is on, the unicode functions parse <b>, <i>, <u>, <p>, <br>, <a href>, <basefont>, <body>,
// <h1>..<h6>, <big>, <small>, <bq>, <left>/<center>/<right> and <div align>, with per-character
// fonts, styles and colors, 18px lines, body margins and background, and link rectangles in
// TextBitmap::links. Prefer HtmlScope so the mode cannot leak into other text.
//
// Not thread-safe: unicode glyphs are decoded lazily on first use, and HTML parsing keeps
// per-call state like the original.
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

    // --- HTML mode ---
    // `startColor` is the text color before any tag (0xAABBGGRR as the original's RGBA
    // reading, 0xFFFFFFFF for "use the hue"); `backgroundCanBeColored` lets <body bgcolor>
    // fill the bitmap background.
    void setUseHtml(bool value, uint32_t startColor = 0xFFFFFFFF, bool backgroundCanBeColored = false);
    bool usingHtml() const { return _useHtml; }

    // Records a clicked link so later renders draw it in the visited-link color. The most
    // recent 1024 URLs are remembered.
    void markUrlVisited(std::string_view url);
    bool urlVisited(std::string_view url) const;

    // Turns HTML mode on for its lifetime and restores the previous mode afterwards.
    class HtmlScope
    {
    public:
        HtmlScope(FontRenderer& fonts, uint32_t startColor = 0xFFFFFFFF, bool backgroundCanBeColored = false);
        ~HtmlScope();
        HtmlScope(const HtmlScope&)            = delete;
        HtmlScope& operator=(const HtmlScope&) = delete;

    private:
        FontRenderer& _fonts;
        bool _wasHtml;
        uint32_t _oldColor;
        bool _oldBackground;
    };

private:
    struct UnicodeFont;
    struct HtmlChar;
    struct HtmlTagInfo;

    struct HtmlMargins
    {
        int x      = 0;
        int y      = 0;
        int width  = 0;
        int height = 0;
    };

    struct HtmlStatus
    {
        uint32_t backgroundColor     = 0;
        uint32_t visitedWebLinkColor = 0;
        uint32_t webLinkColor        = 0;
        uint32_t color               = 0;
        HtmlMargins margins;
        bool backgroundColored = false;
    };

    // Least-recently-used set of visited URLs (ClassicUO VisitedUrlCache).
    class VisitedUrls
    {
    public:
        bool isVisited(const std::string& url);
        void mark(const std::string& url);
        bool contains(const std::string& url) const { return _map.count(url) != 0; }

    private:
        static constexpr std::size_t kCapacity = 1024;
        std::list<std::string> _order;  // most recent first
        std::unordered_map<std::string, std::list<std::string>::iterator> _map;
    };

    TextLayout layoutUnicodeInfo(uint8_t font, std::u16string_view str, TextAlign align, uint16_t flags, int width,
                                 bool countReturns, bool countSpaces, std::vector<std::string>* urls) const;
    TextLayout layoutHtml(uint8_t font, std::u16string_view str, TextAlign align, uint16_t flags, int width,
                          std::vector<std::string>& urls) const;
    int htmlData(std::vector<HtmlChar>& data, uint8_t font, std::u16string_view str, TextAlign align,
                 uint16_t flags, std::vector<std::string>* urls) const;
    void currentHtmlInfo(const std::vector<HtmlTagInfo>& stack, HtmlTagInfo& info) const;
    int parseHtmlTag(std::u16string_view str, int len, int& i, bool& endTag, HtmlTagInfo& info,
                     std::vector<std::string>* urls) const;
    void htmlInfoFromContent(HtmlTagInfo& info, std::u16string_view content, std::vector<std::string>* urls) const;
    uint16_t registerParseUrl(std::vector<std::string>* urls, std::u16string_view link, uint32_t& color) const;
    // Characters left after the tags are parsed out, as GetTextByWidth* measures them.
    int htmlVisibleLength(uint8_t font, std::u16string_view str) const;
    // The HTML branch of GetTextByWidth*: copies as many leading characters as the tags
    // took up into `out` and moves `str` to the tail, as the original does.
    void htmlTextByWidthPrefix(uint8_t font, std::u16string_view& str, int width, bool& isCropped,
                               std::u16string& out, bool unicode) const;
    void resetHtmlStatus() const;

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
    bool _useHtml                = false;
    mutable HtmlStatus _html;
    mutable VisitedUrls _visitedUrls;
};

}  // namespace uo::text
