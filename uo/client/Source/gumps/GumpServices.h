// SPDX-License-Identifier: BSD-2-Clause
// Part of the AxmolUO client. What the gump layer needs from the rest of the client:
// textures for gump and item art, text rendering, clilocs, and a way to talk to the server.
// Each is an interface so the gump code does not depend on who implements it (the asset
// loaders, the Fonts and text thread's TextFactory, the World renderer's hue shader).
#pragma once

#include "axmol/axmol.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace uo::assets
{
class Installation;
}

namespace uo::client::gumps
{

class GumpActions;

// Gump (gumpart) and static item (art) textures, hued on request.
class GumpTextures
{
public:
    virtual ~GumpTextures() = default;

    // nullptr when the id does not exist. Returned textures stay alive for the session.
    virtual ax::Texture2D* gump(uint16_t id, uint16_t hue = 0, bool partialHue = false) = 0;
    virtual ax::Texture2D* art(uint16_t graphic, uint16_t hue = 0, bool partialHue = false) = 0;

    // Pixel-accurate hit tests, top-left origin, like ClassicUO's Contains on gump pics.
    virtual bool gumpOpaqueAt(uint16_t id, int x, int y) = 0;
    virtual bool artOpaqueAt(uint16_t graphic, int x, int y) = 0;

    // tiledata: the item's art is partially hued (only grey pixels take the hue).
    virtual bool artIsPartialHue(uint16_t graphic) = 0;
    // tiledata: the gump (paperdoll) animation id of a wearable item, 0 if none.
    virtual uint16_t artAnimId(uint16_t graphic) = 0;
};

// Mirrors uo::client::text::TextStyle from the Fonts and text thread (TextFactory.h).
struct GumpTextStyle
{
    enum class Align : uint8_t
    {
        Left,
        Center,
        Right,
    };

    uint8_t font = 0xFF;    // 0xFF = client default font
    bool unicode = true;
    uint16_t hue = 0xFFFF;  // 0xFFFF = unhued white
    int maxWidth = 0;       // 0 = no wrap
    bool border = false;    // black outline
    bool cropped = false;   // single line, "..." past maxWidth
    Align align = Align::Left;
    uint16_t extraFlags = 0;
    uint8_t cell = 30;
};

// Text rendering. Nodes come back anchored top-left, content size = rendered size.
class GumpText
{
public:
    virtual ~GumpText() = default;

    virtual ax::Node* createLabel(std::string_view utf8, const GumpTextStyle& style) = 0;
    virtual ax::Node* createHtml(std::string_view html, int width, uint32_t defaultRgba, bool hasBackground) = 0;
    virtual ax::Size measure(std::string_view utf8, const GumpTextStyle& style) = 0;

    // Resolved cliloc text with ~1_ARG~ substitution; empty when unknown.
    virtual std::string cliloc(uint32_t number, std::string_view args = {}) = 0;

    // The colour text of `hue` draws in (text entry carets use it).
    virtual ax::Color32 textColor(uint16_t /*hue*/) const { return ax::Color32::white; }
};

// Colour of unicode text in `hue`: the hue ramp's brightest entry, white when unhued.
ax::Color32 unicodeHueColor(const uo::assets::Installation* assets, uint16_t hue);

// Everything a gump needs, handed to each gump at construction.
struct GumpContext
{
    GumpTextures* textures = nullptr;
    GumpText* text = nullptr;
    // Player actions (use, lift, drop, skills...). May be null for server-gump-only use.
    GumpActions* actions = nullptr;
    // Sends a complete packet (id byte first) to the server.
    std::function<void(std::vector<uint8_t>)> send;
};

// GumpTextures over the core asset loaders, hued on the CPU through hues.mul. Used until the
// hue shader lands; every texture is cached by (id, hue, partial).
class AssetGumpTextures final : public GumpTextures
{
public:
    explicit AssetGumpTextures(const uo::assets::Installation& assets);
    ~AssetGumpTextures() override;

    ax::Texture2D* gump(uint16_t id, uint16_t hue = 0, bool partialHue = false) override;
    ax::Texture2D* art(uint16_t graphic, uint16_t hue = 0, bool partialHue = false) override;
    bool gumpOpaqueAt(uint16_t id, int x, int y) override;
    bool artOpaqueAt(uint16_t graphic, int x, int y) override;
    bool artIsPartialHue(uint16_t graphic) override;
    uint16_t artAnimId(uint16_t graphic) override;

private:
    struct Mask
    {
        int width = 0, height = 0;
        std::vector<bool> bits;
    };

    ax::Texture2D* build(bool isArt, uint16_t id, uint16_t hue, bool partial);
    const Mask& mask(bool isArt, uint16_t id);

    const uo::assets::Installation& _assets;
    std::unordered_map<uint64_t, ax::Texture2D*> _textures;
    std::unordered_map<uint32_t, Mask> _masks;
};

// GumpText with Axmol's system font and tag-stripped HTML, so gumps are usable before the
// UO font renderer is wired in. Hues come from hues.mul.
class FallbackGumpText final : public GumpText
{
public:
    explicit FallbackGumpText(const uo::assets::Installation* assets);

    ax::Node* createLabel(std::string_view utf8, const GumpTextStyle& style) override;
    ax::Node* createHtml(std::string_view html, int width, uint32_t defaultRgba, bool hasBackground) override;
    ax::Size measure(std::string_view utf8, const GumpTextStyle& style) override;
    std::string cliloc(uint32_t number, std::string_view args = {}) override;

    ax::Color32 hueColor(uint16_t hue) const { return unicodeHueColor(_assets, hue); }
    ax::Color32 textColor(uint16_t hue) const override { return hueColor(hue); }

private:
    const uo::assets::Installation* _assets;
};

// Strips tags for plain rendering; <br> and <p> become newlines, entities are decoded.
std::string stripHtml(std::string_view html);

}  // namespace uo::client::gumps
