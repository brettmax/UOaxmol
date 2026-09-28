// SPDX-License-Identifier: BSD-2-Clause
// Ported from ClassicUO (JournalManager.Add, PacketHandlers.DisplayClilocString).

#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace uo::text
{

class FontRenderer;

// The journal line: "name: text", or just the text for system and nameless messages.
std::string journalLine(std::string_view name, std::string_view text);

// Adds a 0xCC affix before or after translated cliloc text; blank affixes are ignored.
std::string applyAffix(std::string text, std::string_view affix, bool prepend);

struct TextFont
{
    std::uint8_t font = 0;
    bool unicode      = true;
};

// Font a journal entry renders with: unicode font 0 for unicode messages, ASCII font 9
// otherwise (JournalManager.Add); forceUnicode is Profile.ForceUnicodeJournal.
TextFont journalFont(bool unicode, bool forceUnicode = false);

// Font overhead text renders with: unicode fonts fall back to font 0 when missing
// (DisplayClilocString); ASCII fonts past the loaded ones fall back to font 3.
TextFont speechFont(std::uint16_t font, bool unicode, const FontRenderer& fonts);

}  // namespace uo::text
