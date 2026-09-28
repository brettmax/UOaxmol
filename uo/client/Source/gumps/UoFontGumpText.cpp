// SPDX-License-Identifier: BSD-2-Clause
#include "gumps/UoFontGumpText.h"

#include "axmol/TextFactory.h"

#include "uo/assets/Installation.h"

namespace uo::client::gumps
{

namespace
{
uo::client::text::TextStyle toTextStyle(const GumpTextStyle& s)
{
    uo::client::text::TextStyle t;
    t.font       = s.font;
    t.unicode    = s.unicode;
    t.hue        = s.hue;
    t.maxWidth   = s.maxWidth;
    t.border     = s.border;
    t.cropped    = s.cropped;
    t.align      = static_cast<uo::text::TextAlign>(s.align);
    t.extraFlags = s.extraFlags;
    t.cell       = s.cell;
    return t;
}
}  // namespace

ax::Node* UoFontGumpText::createLabel(std::string_view utf8, const GumpTextStyle& style)
{
    return uo::client::text::createLabel(utf8, toTextStyle(style));
}

ax::Node* UoFontGumpText::createHtml(std::string_view html, int width, uint32_t defaultRgba, bool hasBackground)
{
    return uo::client::text::createHtml(html, width, defaultRgba, hasBackground);
}

ax::Size UoFontGumpText::measure(std::string_view utf8, const GumpTextStyle& style)
{
    return uo::client::text::measure(utf8, toTextStyle(style));
}

std::string UoFontGumpText::cliloc(uint32_t number, std::string_view args)
{
    return _assets.cliloc().format(static_cast<int32_t>(number), args);
}

}  // namespace uo::client::gumps
