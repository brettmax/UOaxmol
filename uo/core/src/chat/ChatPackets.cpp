// SPDX-License-Identifier: BSD-2-Clause
// Ported from ClassicUO (Network/OutgoingPackets.cs).

#include "uo/chat/ChatPackets.h"

#include "uo/chat/SpeechKeywords.h"
#include "uo/net/PacketWriter.h"
#include "uo/text/Utf.h"

namespace uo::chat::packets
{

using uo::net::PacketWriter;

namespace
{

constexpr std::uint8_t kEncoded = 0xC0;

// Windows-1252 bytes 0x80..0x9F.
constexpr char16_t kCp1252High[32] = {0x20AC, 0,      0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021,
                                      0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0,      0x017D, 0,
                                      0,      0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014,
                                      0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0,      0x017E, 0x0178};

std::uint8_t cp1252Byte(char16_t c)
{
    if (c < 0x80 || (c >= 0xA0 && c <= 0xFF))
    {
        return static_cast<std::uint8_t>(c);
    }
    for (int i = 0; i < 32; ++i)
    {
        if (kCp1252High[i] != 0 && kCp1252High[i] == c)
        {
            return static_cast<std::uint8_t>(0x80 + i);
        }
    }
    return '?';
}

PacketWriter& unicodeLE(PacketWriter& w, std::string_view utf8)
{
    for (const char16_t c : uo::text::utf8ToUtf16(utf8))
    {
        w.u8(static_cast<std::uint8_t>(c & 0xFF)).u8(static_cast<std::uint8_t>(c >> 8));
    }
    return w;
}

PacketWriter party(std::uint8_t command)
{
    PacketWriter w = PacketWriter::variable(0xBF);
    w.u16(0x06).u8(command);
    return w;
}

}  // namespace

std::vector<std::uint8_t> toCp1252(std::string_view utf8)
{
    std::vector<std::uint8_t> out;
    for (const char16_t c : uo::text::utf8ToUtf16(utf8))
    {
        out.push_back(cp1252Byte(c));
    }
    return out;
}

Bytes unicodeSpeech(std::string_view utf8, std::uint8_t type, std::uint8_t font, std::uint16_t hue,
                    std::string_view language, std::span<const std::uint16_t> keywords)
{
    const bool encoded = !keywords.empty();
    PacketWriter w     = PacketWriter::variable(0xAD);
    w.u8(encoded ? static_cast<std::uint8_t>(type | kEncoded) : type).u16(hue).u16(font).ascii(language, 4);

    if (encoded)
    {
        w.bytes(SpeechKeywords::encode(keywords));
        w.bytes(std::span(reinterpret_cast<const std::uint8_t*>(utf8.data()), utf8.size()));
        w.u8(0);
    }
    else
    {
        w.unicodeBE(utf8);
    }
    return w.finish();
}

Bytes asciiSpeech(std::string_view utf8, std::uint8_t type, std::uint8_t font, std::uint16_t hue, bool hasKeywords)
{
    PacketWriter w = PacketWriter::variable(0x03);
    w.u8(hasKeywords ? static_cast<std::uint8_t>(type | kEncoded) : type).u16(hue).u16(font);
    w.bytes(toCp1252(utf8)).u8(0);
    return w.finish();
}

Bytes asciiPromptResponse(std::uint64_t data, std::string_view utf8)
{
    PacketWriter w = PacketWriter::variable(0x9A);
    w.u32(static_cast<std::uint32_t>(data >> 32)).u32(static_cast<std::uint32_t>(data));
    w.u32(utf8.empty() ? 0 : 1);
    w.bytes(toCp1252(utf8)).u8(0);
    return w.finish();
}

Bytes unicodePromptResponse(std::uint64_t data, std::string_view utf8, std::string_view language)
{
    PacketWriter w = PacketWriter::variable(0xC2);
    w.u32(static_cast<std::uint32_t>(data >> 32)).u32(static_cast<std::uint32_t>(data));
    w.u32(utf8.empty() ? 0 : 1);
    w.ascii(language, 3).u8(0);
    unicodeLE(w, utf8);  // the text's own length, no terminator
    return w.finish();
}

Bytes partyMessage(std::string_view utf8, std::uint32_t to)
{
    // A valid serial makes it a private tell (0x03), otherwise it goes to the party (0x04).
    const bool tell = to > 0 && to < 0x80000000u;
    PacketWriter w  = party(tell ? 0x03 : 0x04);
    if (tell)
    {
        w.u32(to);
    }
    w.unicodeBE(utf8);
    return w.finish();
}

Bytes partyInvite()
{
    return party(0x01).u32(0).finish();
}

Bytes partyRemove(std::uint32_t serial)
{
    return party(0x02).u32(serial).finish();
}

Bytes partyAccept(std::uint32_t inviter)
{
    return party(0x08).u32(inviter).finish();
}

Bytes partyDecline(std::uint32_t inviter)
{
    return party(0x09).u32(inviter).finish();
}

}  // namespace uo::chat::packets
