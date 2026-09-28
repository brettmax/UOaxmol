// SPDX-License-Identifier: BSD-2-Clause
// Ported from ClassicUO (RenderedText.cs). Replaces the per-string texture of the
// original with an ax::Sprite over a texture built from uocore's text bitmap.

#pragma once

#include "uo/text/TextTypes.h"

#include "axmol/2d/Sprite.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace uo::client::text
{

struct TextStyle
{
    std::uint8_t font = 0xFF;  // 0xFF = client default (1 on 3.0.5d+, else 0)
    bool unicode      = true;
    std::uint16_t hue = 0xFFFF;  // 0xFFFF = unhued white
    int maxWidth      = 0;       // 0 = no wrap
    bool border       = false;   // black outline (FontStyleBlackBorder)
    bool cropped      = false;   // single line, "..." past maxWidth
    uo::text::TextAlign align = uo::text::TextAlign::Left;
    std::uint16_t extraFlags  = 0;   // other uo::text::FontStyle bits (solid, italic, underline...)
    std::uint8_t cell         = 30;  // hue ramp entry for unicode text
    bool saveHitMap           = false;  // keep an alpha mask for pixel-exact hitTest
    // Gump HTML (unicode fonts only): tags are parsed as the original client does.
    bool html                  = false;
    std::uint32_t htmlColor    = 0xFFFFFFFF;  // text color before any tag, see createHtml
    bool htmlBackgroundColored = false;       // let <body bgcolor> fill the background

    bool operator==(const TextStyle&) const = default;

    std::uint16_t flags() const
    {
        return static_cast<std::uint16_t>(extraFlags | (border ? uo::text::FontStyleBlackBorder : 0) |
                                          (cropped ? uo::text::FontStyleCropped : 0));
    }
};

// The UTF-16 string the font renderer sees: full UTF-16 for unicode fonts; for ASCII fonts
// each code point's low byte, or the raw bytes when the text is not valid UTF-8 (0x1C speech).
std::u16string fontText(std::string_view utf8, bool unicode);

// Renders `utf8` with the UO fonts. Unicode fonts use the full UTF-16 text; ASCII fonts
// get each code point's low byte, as the original client does.
uo::text::TextBitmap renderText(std::string_view utf8, const TextStyle& style);

// Laid-out size of `utf8` without building a texture.
ax::Size measure(std::string_view utf8, const TextStyle& style);

// Uploads a text bitmap as a nearest-filtered RGBA8 texture (autoreleased), or null when empty.
ax::Texture2D* createTexture(const uo::text::TextBitmap& bitmap);

// A sprite showing UO-font text, anchored at its top-left corner. Its content size is the
// rendered bitmap's, which includes the original's 4-pixel right margin.
class TextLabel : public ax::Sprite
{
public:
    static TextLabel* create(std::string_view utf8, const TextStyle& style);

    const std::string& text() const { return _text; }
    const TextStyle& style() const { return _style; }
    int lineCount() const { return _lineCount; }

    void setText(std::string_view utf8);
    void setStyle(const TextStyle& style);
    void setHue(std::uint16_t hue);
    // Text and style together, rebuilt once and only when either changed.
    void setContent(std::string_view utf8, const TextStyle& style);

    // Local point in UO orientation (origin top-left, y down). With saveHitMap only opaque
    // pixels hit, as PixelCheck does; otherwise the bounding box does.
    bool hitTest(int x, int y) const;

    // HTML labels: the <a href> areas, in the same local UO coordinates as hitTest, and the
    // URL under a point (null when none). Mark a clicked URL with markLinkVisited so it
    // redraws in the visited color.
    const std::vector<uo::text::WebLinkRect>& links() const { return _links; }
    const std::string* linkAt(int x, int y) const;
    void markLinkVisited(const std::string& url);

    bool init(std::string_view utf8, const TextStyle& style);

private:
    void rebuild();

    std::string _text;
    TextStyle _style;
    int _lineCount = 0;
    int _hitWidth  = 0;
    std::vector<bool> _hitMask;
    std::vector<uo::text::WebLinkRect> _links;
};

inline ax::Node* createLabel(std::string_view utf8, const TextStyle& style)
{
    return TextLabel::create(utf8, style);
}

// Gump HTML text (htmlgump, xmfhtmlgump) in unicode font 1 wrapped at `width`, laid out by
// the original client's HTML renderer: <b>, <i>, <u>, <p>, <br>, <a href>, <basefont>,
// <body>, <h1>..<h6>, <big>, <small>, <bq>, <left>/<center>/<right>, <div align>.
// `defaultRgba` (0xRRGGBBAA) colors text outside any color tag; 0xFFFFFFFF keeps the
// unhued white. As in ClassicUO's HtmlControl, <body bgcolor> paints only when the control
// has no background of its own. Returns a TextLabel; its links() are clickable areas.
ax::Node* createHtml(std::string_view html, int width, std::uint32_t defaultRgba, bool hasBackground);

// Plain text with the markup removed (<br> and <p> break lines, entities decode), for
// places that show gump HTML without formatting, such as tooltips and logs.
std::string stripHtml(std::string_view html);

// The HTML start color for a 0xRRGGBBAA color, in the byte order the renderer reads.
constexpr std::uint32_t htmlStartColor(std::uint32_t rgba)
{
    return ((rgba >> 8) & 0xFF) << 24 | ((rgba >> 16) & 0xFF) << 16 | ((rgba >> 24) & 0xFF) << 8 | (rgba & 0xFF);
}

}  // namespace uo::client::text
