// SPDX-License-Identifier: BSD-2-Clause
// Ported from ClassicUO (JournalManager.Add, PacketHandlers.DisplayClilocString).

#include "uo/text/JournalText.h"

#include "uo/text/FontRenderer.h"

#include <algorithm>

namespace uo::text
{

namespace
{
bool isBlank(std::string_view s)
{
    return std::all_of(s.begin(), s.end(), [](char c) {
        return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
    });
}
}  // namespace

std::string journalLine(std::string_view name, std::string_view text)
{
    if (isBlank(name))
        return std::string(text);

    std::string line(name);
    line += ": ";
    line += text;
    return line;
}

std::string applyAffix(std::string text, std::string_view affix, bool prepend)
{
    if (isBlank(affix))
        return text;

    return prepend ? std::string(affix) + text : text + std::string(affix);
}

TextFont journalFont(bool unicode, bool forceUnicode)
{
    if (forceUnicode || unicode)
        return {0, true};

    return {9, false};
}

TextFont speechFont(std::uint16_t font, bool unicode, const FontRenderer& fonts)
{
    auto f = static_cast<std::uint8_t>(font);

    if (unicode)
        return {fonts.unicodeFontExists(f) ? f : std::uint8_t{0}, true};

    if (font >= fonts.asciiFontCount())
        f = 3;

    return {f, false};
}

}  // namespace uo::text
