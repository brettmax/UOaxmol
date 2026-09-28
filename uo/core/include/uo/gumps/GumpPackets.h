// SPDX-License-Identifier: BSD-2-Clause
// Part of the AxmolUO client core.
//
// Wire formats for server gumps:
//   0xB0  open gump (uncompressed layout and text)
//   0xDD  open compressed gump (zlib layout and zlib text table)
//   0xB1  gump response (client -> server)
//
// Ported from ClassicUO's PacketHandlers.OpenGump / OpenCompressedGump and
// OutgoingPackets.Send_GumpResponse.

#pragma once

#include "uo/gumps/GumpLayout.h"

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace uo::gumps
{

// The raw pieces of an open-gump packet, before the layout is parsed.
struct GumpPacket
{
    uint32_t sender = 0;
    uint32_t gumpId = 0;
    int32_t x = 0;
    int32_t y = 0;
    std::string layout;              // layout language text
    std::vector<std::string> lines;  // text table, UTF-8
};

// Both decoders take the whole packet including the id byte and the 16-bit
// length. They return std::nullopt on truncation or a zlib failure.
std::optional<GumpPacket> decodeOpenGump(std::span<const uint8_t> packet);            // 0xB0
std::optional<GumpPacket> decodeOpenCompressedGump(std::span<const uint8_t> packet);  // 0xDD

// Decodes either packet by its id byte, then parses the layout.
std::optional<GumpLayout> decodeGump(std::span<const uint8_t> packet);

// Builds the full 0xB1 packet. `switches` are the ids of checked checkboxes and
// radios; `entries` are (text entry id, UTF-8 text). Entry text is truncated to
// 239 UTF-16 units like the classic client.
std::vector<uint8_t> encodeGumpResponse(uint32_t sender, uint32_t gumpId, uint32_t buttonId,
                                        std::span<const uint32_t> switches,
                                        std::span<const std::pair<uint16_t, std::string>> entries);

// UTF-8 to UTF-16 code units (surrogate pairs for astral code points).
std::u16string utf8ToUtf16(std::string_view utf8);

}  // namespace uo::gumps
