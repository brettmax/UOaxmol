// SPDX-License-Identifier: BSD-2-Clause
// Ported from ClassicUO (RenderedText.cs).

#include "axmol/TextFactory.h"

#include "axmol/TextSystem.h"

#include "uo/text/Utf.h"

#include "axmol/renderer/Texture2D.h"

#include <cctype>

namespace uo::client::text
{

namespace
{

// ASCII speech arrives as raw single-byte text, which is usually not valid UTF-8.
bool isValidUtf8(std::string_view s)
{
    return uo::text::utf16ToUtf8(uo::text::utf8ToUtf16(s)) == s;
}

}  // namespace

std::u16string fontText(std::string_view utf8, bool unicode)
{
    if (!unicode && !isValidUtf8(utf8))
    {
        std::u16string bytes;
        for (char c : utf8)
            bytes.push_back(static_cast<unsigned char>(c));
        return bytes;
    }

    std::u16string s = uo::text::utf8ToUtf16(utf8);

    if (!unicode)
    {
        for (auto& c : s)
            c = static_cast<char16_t>(c & 0xFF);
    }

    return s;
}

uo::text::TextBitmap renderText(std::string_view utf8, const TextStyle& style)
{
    auto& system = TextSystem::instance();

    if (!system.ready() || utf8.empty())
        return {};

    const auto& fonts       = system.fonts();
    const std::uint8_t font = system.resolveFont(style.font);
    const std::u16string s  = fontText(utf8, style.unicode);

    if (style.unicode)
        return fonts.generateUnicode(font, s, style.hue, style.cell, style.maxWidth, style.align, style.flags());

    return fonts.generateAscii(font, s, style.hue, style.maxWidth, style.align, style.flags());
}

ax::Size measure(std::string_view utf8, const TextStyle& style)
{
    auto& system = TextSystem::instance();

    if (!system.ready() || utf8.empty())
        return ax::Size::zero;

    const auto& fonts       = system.fonts();
    const std::uint8_t font = system.resolveFont(style.font);
    const std::u16string s  = fontText(utf8, style.unicode);
    const auto flags        = style.flags();

    // Mirrors the bitmap sizes of generateAscii/generateUnicode: the laid-out width (or
    // maxWidth) plus a 4-pixel margin, and for unicode a 4-pixel bottom margin.
    int width = style.maxWidth;

    if (style.unicode)
    {
        if (width <= 0)
            width = fonts.widthUnicode(font, s);

        const int height = fonts.heightUnicode(font, s, width, style.align, flags);
        return height > 0 ? ax::Size(static_cast<float>(width + 4), static_cast<float>(height + 4)) : ax::Size::zero;
    }

    if (width <= 0)
        width = fonts.widthAscii(font, s);

    const int height = fonts.heightAscii(font, s, width, style.align, flags);
    return height > 0 ? ax::Size(static_cast<float>(width + 4), static_cast<float>(height)) : ax::Size::zero;
}

ax::Texture2D* createTexture(const uo::text::TextBitmap& bitmap)
{
    if (bitmap.empty())
        return nullptr;

    auto* texture = new ax::Texture2D();

    if (!texture->initWithData(bitmap.pixels.data(), static_cast<ssize_t>(bitmap.pixels.size() * 4),
                               ax::rhi::PixelFormat::RGBA8, bitmap.width, bitmap.height, false))
    {
        texture->release();
        return nullptr;
    }

    // Bitmap fonts: never filter between texels.
    texture->setAliasTexParameters();
    texture->autorelease();
    return texture;
}

TextLabel* TextLabel::create(std::string_view utf8, const TextStyle& style)
{
    auto* label = new TextLabel();

    if (label->init(utf8, style))
    {
        label->autorelease();
        return label;
    }

    delete label;
    return nullptr;
}

bool TextLabel::init(std::string_view utf8, const TextStyle& style)
{
    if (!ax::Sprite::init())
        return false;

    setAnchorPoint(ax::Vec2(0.0f, 1.0f));
    setBlendFunc(ax::BlendFunc::ALPHA_NON_PREMULTIPLIED);
    _text  = std::string(utf8);
    _style = style;
    rebuild();
    return true;
}

void TextLabel::setText(std::string_view utf8)
{
    if (_text == utf8)
        return;

    _text = std::string(utf8);
    rebuild();
}

void TextLabel::setStyle(const TextStyle& style)
{
    _style = style;
    rebuild();
}

void TextLabel::setHue(std::uint16_t hue)
{
    if (_style.hue == hue)
        return;

    _style.hue = hue;
    rebuild();
}

void TextLabel::rebuild()
{
    const uo::text::TextBitmap bitmap = renderText(_text, _style);
    _lineCount = bitmap.lineCount;
    _hitMask.clear();
    _hitWidth = 0;

    if (_style.saveHitMap && !bitmap.empty())
    {
        _hitWidth = bitmap.width;
        _hitMask.resize(bitmap.pixels.size());

        for (size_t i = 0; i < bitmap.pixels.size(); ++i)
            _hitMask[i] = bitmap.pixels[i] != 0;
    }

    if (ax::Texture2D* texture = createTexture(bitmap))
    {
        setTexture(texture);
        setTextureRect(ax::Rect(0, 0, static_cast<float>(bitmap.width), static_cast<float>(bitmap.height)));
        setVisible(true);
    }
    else
    {
        setTexture(nullptr);
        setTextureRect(ax::Rect::zero);
        setVisible(false);
    }

    // setTexture may reset the blend function to the texture's default.
    setBlendFunc(ax::BlendFunc::ALPHA_NON_PREMULTIPLIED);
}

bool TextLabel::hitTest(int x, int y) const
{
    const ax::Size& size = getContentSize();

    if (_text.empty() || x < 0 || y < 0 || x >= static_cast<int>(size.width) || y >= static_cast<int>(size.height))
        return false;

    if (!_style.saveHitMap || _hitWidth == 0)
        return true;

    const size_t index = static_cast<size_t>(y) * _hitWidth + x;
    return index < _hitMask.size() && _hitMask[index];
}

std::string stripHtml(std::string_view html)
{
    std::string out;
    out.reserve(html.size());

    auto lower = [](std::string_view s) {
        std::string r(s);
        for (auto& c : r)
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return r;
    };

    for (size_t i = 0; i < html.size(); ++i)
    {
        const char c = html[i];

        if (c == '<')
        {
            const size_t close = html.find('>', i + 1);

            if (close == std::string_view::npos)
            {
                out.append(html.substr(i));
                break;
            }

            std::string tag = lower(html.substr(i + 1, close - i - 1));
            while (!tag.empty() && (tag.front() == '/' || tag.front() == ' '))
                tag.erase(tag.begin());

            const std::string name = tag.substr(0, tag.find_first_of(" /"));

            if (name == "br" || (name == "p" && html[i + 1] != '/'))
                out.push_back('\n');

            i = close;
        }
        else if (c == '&')
        {
            static constexpr std::pair<std::string_view, std::string_view> kEntities[] = {
                {"&amp;", "&"}, {"&lt;", "<"}, {"&gt;", ">"}, {"&quot;", "\""}, {"&apos;", "'"}, {"&nbsp;", " "}};
            bool matched = false;

            for (const auto& [entity, value] : kEntities)
            {
                if (lower(html.substr(i, entity.size())) == entity)
                {
                    out.append(value);
                    i += entity.size() - 1;
                    matched = true;
                    break;
                }
            }

            if (!matched)
                out.push_back(c);
        }
        else
        {
            out.push_back(c);
        }
    }

    return out;
}

ax::Node* createHtml(std::string_view html, int width, std::uint32_t defaultRgba, bool hasBackground)
{
    TextStyle style;
    style.font     = 1;
    style.unicode  = true;
    style.maxWidth = width;

    auto* label = TextLabel::create(stripHtml(html), style);

    if (!label)
        return nullptr;

    // The text renders white, so the sprite color carries the HTML default color.
    label->setColor(ax::Color32(static_cast<uint8_t>(defaultRgba >> 24), static_cast<uint8_t>(defaultRgba >> 16),
                                static_cast<uint8_t>(defaultRgba >> 8)));
    label->setOpacity(static_cast<uint8_t>(defaultRgba & 0xFF));
    (void)hasBackground;  // background fill arrives with the HTML port
    return label;
}

}  // namespace uo::client::text
