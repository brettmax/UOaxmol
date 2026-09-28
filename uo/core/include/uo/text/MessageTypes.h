// SPDX-License-Identifier: BSD-2-Clause
// Ported from ClassicUO (MessageType.cs, TextType.cs).

#pragma once

#include <cstdint>

// Once the uo::world game state is in uocore these are its types, so world messages pass
// straight through; until then the same enums are defined here. Remove this header when
// uo/world is on every branch that uses it.
#if __has_include("uo/world/Types.h")
#include "uo/world/Types.h"

namespace uo::text
{
using world::MessageType;
using world::TextType;
}  // namespace uo::text

#else

namespace uo::text
{

// Speech/message type byte from the 0x1C, 0xAE and 0xC1 packets.
enum class MessageType : uint8_t
{
    Regular    = 0,
    System     = 1,
    Emote      = 2,
    Limit3Spell = 3,
    Label      = 6,
    Focus      = 7,
    Whisper    = 8,
    Yell       = 9,
    Spell      = 10,
    Guild      = 13,
    Alliance   = 14,
    Command    = 15,
    GmChat     = 16,
    Encoded    = 0xC0,
    Party      = 0xFF,  // assigned by the client, not sent by servers
};

// Where a message came from, which decides how it is shown.
enum class TextType : uint8_t
{
    Client,
    System,
    Object,
    GuildAlly,
};

}  // namespace uo::text

#endif
