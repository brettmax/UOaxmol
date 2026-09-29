// SPDX-License-Identifier: BSD-2-Clause
// Ported from ClassicUO (Network/OutgoingPackets.cs): the packets the chat line sends.

#pragma once

#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace uo::chat::packets
{

using Bytes = std::vector<std::uint8_t>;

// 0xAD unicode speech. With keyword ids the type gains 0xC0 and the text goes out as the
// keyword block, UTF-8 and a NUL (Send_UnicodeSpeechRequest); without, as UTF-16BE.
Bytes unicodeSpeech(std::string_view utf8, std::uint8_t type, std::uint8_t font, std::uint16_t hue,
                    std::string_view language, std::span<const std::uint16_t> keywords);

// 0x03 ASCII speech, for clients before 2.0.0 (Send_ASCIISpeechRequest).
Bytes asciiSpeech(std::string_view utf8, std::uint8_t type, std::uint8_t font, std::uint16_t hue,
                  bool hasKeywords);

// 0x9A / 0xC2 prompt answers; an empty text cancels the prompt.
Bytes asciiPromptResponse(std::uint64_t data, std::string_view utf8);
Bytes unicodePromptResponse(std::uint64_t data, std::string_view utf8, std::string_view language);

// 0xBF 0x06 party commands.
Bytes partyMessage(std::string_view utf8, std::uint32_t to);  // `to` 0: everyone
Bytes partyInvite();                                          // opens the server's target
Bytes partyRemove(std::uint32_t serial);                      // 0: target someone to remove
Bytes partyAccept(std::uint32_t inviter);
Bytes partyDecline(std::uint32_t inviter);

// UTF-8 to Windows-1252 bytes; characters it has no byte for become '?'.
std::vector<std::uint8_t> toCp1252(std::string_view utf8);

}  // namespace uo::chat::packets
