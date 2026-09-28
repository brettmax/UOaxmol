// SPDX-License-Identifier: BSD-2-Clause
#include "uo/net/OutgoingPackets.h"

#include "uo/net/PacketWriter.h"

namespace uo::net::out
{

Bytes seed(std::uint32_t s, ClientVersion v)
{
    return PacketWriter(0xEF, 21).u32(s).u32(v >> 24).u32((v >> 16) & 0xFF).u32((v >> 8) & 0xFF).u32(v & 0xFF).finish();
}

Bytes seedOld(std::uint32_t s)
{
    return {static_cast<std::uint8_t>(s >> 24), static_cast<std::uint8_t>(s >> 16), static_cast<std::uint8_t>(s >> 8),
            static_cast<std::uint8_t>(s)};
}

Bytes accountLogin(std::string_view account, std::string_view password)
{
    return PacketWriter(0x80, 62).ascii(account, 30).ascii(password, 30).u8(0xFF).finish();
}

Bytes selectShard(std::uint16_t index)
{
    return PacketWriter(0xA0, 3).u16(index).finish();
}

Bytes gameLogin(std::uint32_t authKey, std::string_view account, std::string_view password)
{
    return PacketWriter(0x91, 65).u32(authKey).ascii(account, 30).ascii(password, 30).finish();
}

Bytes selectCharacter(std::uint32_t slot, std::string_view name, std::uint32_t clientIp, std::uint32_t protocol)
{
    return PacketWriter(0x5D, 73)
        .u32(0xEDEDEDED)
        .ascii(name, 30)
        .zero(2)
        .u32(protocol)
        .zero(24)
        .u32(slot)
        .u32(clientIp)
        .finish();
}

Bytes clientVersion(std::string_view version)
{
    return PacketWriter::variable(0xBD).ascii(version).finish();
}

Bytes ping(std::uint8_t sequence)
{
    return PacketWriter(0x73, 2).u8(sequence).finish();
}

Bytes walkRequest(std::uint8_t direction, std::uint8_t sequence, std::uint32_t fastWalkKey)
{
    return PacketWriter(0x02, 7).u8(direction).u8(sequence).u32(fastWalkKey).finish();
}

Bytes doubleClick(std::uint32_t serial)
{
    return PacketWriter(0x06, 5).u32(serial).finish();
}

Bytes singleClick(std::uint32_t serial)
{
    return PacketWriter(0x09, 5).u32(serial).finish();
}

Bytes resync()
{
    return PacketWriter(0x22, 3).finish();
}

Bytes speech(std::string_view utf8, std::uint8_t type, std::uint16_t hue, std::uint16_t font, std::string_view language)
{
    return PacketWriter::variable(0xAD).u8(type).u16(hue).u16(font).ascii(language, 4).unicodeBE(utf8).finish();
}

Bytes statusRequest(std::uint32_t serial)
{
    return PacketWriter(0x34, 10).u32(0xEDEDEDED).u8(4).u32(serial).finish();
}

}  // namespace uo::net::out
