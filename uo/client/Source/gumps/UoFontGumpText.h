// SPDX-License-Identifier: BSD-2-Clause
// Part of the AxmolUO client. GumpText over the UO font renderer (uo/client/text), used in
// place of FallbackGumpText once fonts.mul has loaded.
#pragma once

#include "gumps/GumpServices.h"

namespace uo::client::gumps
{

class UoFontGumpText final : public GumpText
{
public:
    explicit UoFontGumpText(const uo::assets::Installation& assets) : _assets(assets) {}

    ax::Node* createLabel(std::string_view utf8, const GumpTextStyle& style) override;
    ax::Node* createHtml(std::string_view html, int width, uint32_t defaultRgba, bool hasBackground) override;
    ax::Size measure(std::string_view utf8, const GumpTextStyle& style) override;
    std::string cliloc(uint32_t number, std::string_view args = {}) override;

private:
    const uo::assets::Installation& _assets;
};

}  // namespace uo::client::gumps
