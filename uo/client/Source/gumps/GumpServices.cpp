// SPDX-License-Identifier: BSD-2-Clause
#include "gumps/GumpServices.h"

#include "uo/assets/Color.h"
#include "uo/assets/Installation.h"

namespace uo::client::gumps
{

namespace
{

uint64_t textureKey(bool isArt, uint16_t id, uint16_t hue, bool partial)
{
    return (uint64_t{isArt} << 40) | (uint64_t{partial} << 32) | (uint64_t{hue} << 16) | id;
}

// Recover the 15-bit colour from an expanded RGBA8 pixel (kColor5To8 is invertible by >> 3).
uint16_t to16(uint32_t rgba)
{
    uint32_t r = (rgba & 0xFF) >> 3, g = ((rgba >> 8) & 0xFF) >> 3, b = ((rgba >> 16) & 0xFF) >> 3;
    return static_cast<uint16_t>((r << 10) | (g << 5) | b);
}

constexpr const char* kFallbackFont = "fonts/arial.ttf";
constexpr float kFallbackFontSize = 12.0f;

}  // namespace

AssetGumpTextures::AssetGumpTextures(const uo::assets::Installation& assets) : _assets(assets) {}

AssetGumpTextures::~AssetGumpTextures()
{
    for (auto& [key, tex] : _textures)
    {
        AX_SAFE_RELEASE(tex);
    }
}

ax::Texture2D* AssetGumpTextures::build(bool isArt, uint16_t id, uint16_t hue, bool partial)
{
    uo::assets::Image img = isArt ? _assets.art().statik(id) : _assets.gumps().get(id);

    if (img.empty())
    {
        return nullptr;
    }

    if (hue != 0)
    {
        const auto& hues = _assets.hues();

        for (auto& px : img.pixels)
        {
            if (px == 0)
            {
                continue;
            }

            uint16_t c = to16(px);
            px = (partial ? hues.applyPartialHue(c, hue) : hues.applyHue(c, hue)) | uo::assets::kOpaque;
        }
    }

    auto* tex = new ax::Texture2D();
    tex->initWithData(img.pixels.data(), static_cast<ssize_t>(img.pixels.size() * 4), ax::rhi::PixelFormat::RGBA8,
                      img.width, img.height);
    // Pixel art: no smoothing when the UI is scaled.
    tex->setAliasTexParameters();
    return tex;
}

ax::Texture2D* AssetGumpTextures::gump(uint16_t id, uint16_t hue, bool partialHue)
{
    uint64_t key = textureKey(false, id, hue, partialHue);

    if (auto it = _textures.find(key); it != _textures.end())
    {
        return it->second;
    }

    return _textures[key] = build(false, id, hue, partialHue);
}

ax::Texture2D* AssetGumpTextures::art(uint16_t graphic, uint16_t hue, bool partialHue)
{
    uint64_t key = textureKey(true, graphic, hue, partialHue);

    if (auto it = _textures.find(key); it != _textures.end())
    {
        return it->second;
    }

    return _textures[key] = build(true, graphic, hue, partialHue);
}

const AssetGumpTextures::Mask& AssetGumpTextures::mask(bool isArt, uint16_t id)
{
    uint32_t key = (uint32_t{isArt} << 16) | id;

    if (auto it = _masks.find(key); it != _masks.end())
    {
        return it->second;
    }

    Mask m;
    uo::assets::Image img = isArt ? _assets.art().statik(id) : _assets.gumps().get(id);
    m.width = img.width;
    m.height = img.height;
    m.bits.resize(img.pixels.size());

    for (size_t i = 0; i < img.pixels.size(); ++i)
    {
        m.bits[i] = img.pixels[i] != 0;
    }

    return _masks[key] = std::move(m);
}

bool AssetGumpTextures::gumpOpaqueAt(uint16_t id, int x, int y)
{
    const Mask& m = mask(false, id);
    return x >= 0 && y >= 0 && x < m.width && y < m.height && m.bits[static_cast<size_t>(y) * m.width + x];
}

bool AssetGumpTextures::artOpaqueAt(uint16_t graphic, int x, int y)
{
    const Mask& m = mask(true, graphic);
    return x >= 0 && y >= 0 && x < m.width && y < m.height && m.bits[static_cast<size_t>(y) * m.width + x];
}

bool AssetGumpTextures::artIsPartialHue(uint16_t graphic)
{
    const auto* t = _assets.tileData().staticTile(graphic);
    return t && t->is(uo::assets::TF_PartialHue);
}

uint16_t AssetGumpTextures::artAnimId(uint16_t graphic)
{
    const auto* t = _assets.tileData().staticTile(graphic);
    return t ? t->animId : 0;
}

FallbackGumpText::FallbackGumpText(const uo::assets::Installation* assets) : _assets(assets) {}

ax::Color32 FallbackGumpText::hueColor(uint16_t hue) const
{
    if (hue == 0xFFFF || hue == 0 || !_assets)
    {
        return ax::Color32::white;
    }

    // White through the hue ramp gives the hue's brightest entry, which is how the classic
    // client colours unicode text.
    uint32_t c = _assets->hues().applyHue(0x7FFF, hue);
    return ax::Color32(c & 0xFF, (c >> 8) & 0xFF, (c >> 16) & 0xFF, 0xFF);
}

ax::Node* FallbackGumpText::createLabel(std::string_view utf8, const GumpTextStyle& style)
{
    auto align = style.align == GumpTextStyle::Align::Center  ? ax::TextHAlignment::CENTER
                 : style.align == GumpTextStyle::Align::Right ? ax::TextHAlignment::RIGHT
                                                              : ax::TextHAlignment::LEFT;
    auto* label = ax::Label::createWithTTF(utf8, kFallbackFont, kFallbackFontSize,
                                           ax::Vec2(static_cast<float>(style.maxWidth), 0), align);

    if (!label)
    {
        label = ax::Label::createWithSystemFont(utf8, "sans", kFallbackFontSize);
    }

    if (style.cropped && style.maxWidth > 0)
    {
        label->setDimensions(static_cast<float>(style.maxWidth), kFallbackFontSize + 4);
        label->setOverflow(ax::Label::Overflow::CLAMP);
    }

    label->setTextColor(hueColor(style.hue));

    if (style.border)
    {
        label->enableOutline(ax::Color32::black, 1);
    }

    label->setAnchorPoint(ax::Vec2(0, 1));
    return label;
}

ax::Node* FallbackGumpText::createHtml(std::string_view html, int width, uint32_t defaultRgba, bool hasBackground)
{
    GumpTextStyle style;
    style.maxWidth = width;
    auto* label = static_cast<ax::Label*>(createLabel(stripHtml(html), style));
    label->setTextColor(ax::Color32((defaultRgba >> 24) & 0xFF, (defaultRgba >> 16) & 0xFF,
                                    (defaultRgba >> 8) & 0xFF, 0xFF));
    return label;
}

ax::Size FallbackGumpText::measure(std::string_view utf8, const GumpTextStyle& style)
{
    auto* n = createLabel(utf8, style);
    return n->getContentSize();
}

std::string FallbackGumpText::cliloc(uint32_t number, std::string_view args)
{
    if (!_assets)
    {
        return {};
    }

    return _assets->cliloc().format(static_cast<int32_t>(number), args);
}

std::string stripHtml(std::string_view html)
{
    std::string out;
    out.reserve(html.size());

    for (size_t i = 0; i < html.size();)
    {
        char c = html[i];

        if (c == '<')
        {
            size_t end = html.find('>', i);

            if (end == std::string_view::npos)
            {
                break;
            }

            std::string tag;

            for (size_t k = i + 1; k < end && html[k] != ' '; ++k)
            {
                char t = html[k];
                tag += static_cast<char>(t >= 'A' && t <= 'Z' ? t - 'A' + 'a' : t);
            }

            if (tag == "br" || tag == "br/" || tag == "p" || tag == "/p" || tag == "/div")
            {
                out += '\n';
            }

            i = end + 1;
        }
        else if (c == '&')
        {
            static constexpr std::pair<std::string_view, char> entities[] = {
                {"&lt;", '<'}, {"&gt;", '>'}, {"&amp;", '&'}, {"&quot;", '"'}, {"&apos;", '\''}, {"&nbsp;", ' '}};
            bool matched = false;

            for (auto [name, ch] : entities)
            {
                if (html.substr(i, name.size()) == name)
                {
                    out += ch;
                    i += name.size();
                    matched = true;
                    break;
                }
            }

            if (!matched)
            {
                out += c;
                ++i;
            }
        }
        else
        {
            out += c;
            ++i;
        }
    }

    return out;
}

}  // namespace uo::client::gumps
