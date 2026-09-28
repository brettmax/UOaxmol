// SPDX-License-Identifier: BSD-2-Clause
//
// The client's text services: the loaded UO fonts, the cliloc table and the hue lookups
// the font renderer needs. Initialise once after the installation has loaded:
//   TextSystem::instance().init(installation);

#pragma once

#include "uo/text/FontRenderer.h"
#include "uo/text/HueResolver.h"

#include <cstdint>
#include <memory>

namespace uo::assets
{
class Cliloc;
class Installation;
}  // namespace uo::assets

namespace uo::client::text
{

class TextSystem
{
public:
    static TextSystem& instance();

    // Loads fonts.mul and unifont*.mul and takes hues and clilocs from the installation,
    // which must outlive the text system. Returns false when fonts.mul is missing.
    bool init(const assets::Installation& installation);
    void shutdown();

    bool ready() const { return _installation != nullptr; }

    const uo::text::FontRenderer& fonts() const { return _fonts; }
    // Mutable access for HTML mode and visited links.
    uo::text::FontRenderer& fonts() { return _fonts; }
    const assets::Cliloc& cliloc() const;

    // The font behind RenderedText's 0xFF: unicode font 1 on 3.0.5d+ clients, else 0.
    std::uint8_t defaultFont() const { return _defaultFont; }
    std::uint8_t resolveFont(std::uint8_t font) const { return font == 0xFF ? _defaultFont : font; }

private:
    TextSystem() = default;

    uo::text::FontRenderer _fonts;
    std::unique_ptr<uo::text::HuesResolver> _hues;
    const assets::Installation* _installation = nullptr;
    std::uint8_t _defaultFont = 1;
};

}  // namespace uo::client::text
