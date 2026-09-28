// SPDX-License-Identifier: BSD-2-Clause
// Part of the AxmolUO client core. Ported from ClassicUO (SoundsLoader, PacketHandlers).
//
// Server -> client audio packets, parsed from the bytes after the packet id.
//
//   0x54 Play Sound Effect (12 bytes): mode u8, sound u16, volume u16, x u16, y u16, z i16
//   0x6D Play Music        (3 bytes):  music u16; 0x1F 0xFF means stop
//
// Framing belongs to the network layer; these take the payload so the handlers stay testable.

#pragma once

#include <cstdint>
#include <optional>
#include <span>

namespace uo::sound
{

inline constexpr uint8_t kPacketPlaySound = 0x54;
inline constexpr uint8_t kPacketPlayMusic = 0x6D;

struct PlaySoundPacket
{
    uint8_t mode   = 0;  // 0 = play once, 1 = repeat (ClassicUO ignores it)
    uint16_t sound = 0;
    uint16_t volume = 0;  // unused by the client
    uint16_t x     = 0;
    uint16_t y     = 0;
    int16_t z      = 0;
};

struct PlayMusicPacket
{
    bool stop     = false;
    uint16_t music = 0;
};

inline std::optional<PlaySoundPacket> parsePlaySound(std::span<const uint8_t> p)
{
    if (p.size() < 11)
    {
        return std::nullopt;
    }

    auto be16 = [&](size_t at) { return static_cast<uint16_t>((p[at] << 8) | p[at + 1]); };

    PlaySoundPacket out;
    out.mode   = p[0];
    out.sound  = be16(1);
    out.volume = be16(3);
    out.x      = be16(5);
    out.y      = be16(7);
    out.z      = static_cast<int16_t>(be16(9));
    return out;
}

inline std::optional<PlayMusicPacket> parsePlayMusic(std::span<const uint8_t> p)
{
    if (p.size() < 2)
    {
        return std::nullopt;
    }

    PlayMusicPacket out;

    if (p[0] == 0x1F && p[1] == 0xFF)
    {
        out.stop = true;
        return out;
    }

    // ClassicUO plays only the low byte for the 3-byte packet; ids never exceed 150 anyway.
    out.music = p[1];
    return out;
}

}  // namespace uo::sound
