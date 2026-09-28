// SPDX-License-Identifier: BSD-2-Clause
// Ported from ClassicUO (Network/PacketHandlers.cs: DenyWalk, ConfirmWalk, MovePlayer, ExtendedCommand).

#include "uo/movement/MovementPackets.h"

#include "uo/io/BinaryReader.h"

namespace uo::movement::packets
{

std::optional<DenyWalk> parseDenyWalk(std::span<const uint8_t> packet)
{
    io::BinaryReader r(packet);
    if (r.readU8() != kDenyWalk)
    {
        return std::nullopt;
    }

    DenyWalk out;
    out.sequence = r.readU8();
    out.x = r.readU16BE();
    out.y = r.readU16BE();
    out.direction = masked(toDirection(r.readU8()));
    out.z = r.readI8();
    return r.overflowed() ? std::nullopt : std::optional(out);
}

std::optional<ConfirmWalk> parseConfirmWalk(std::span<const uint8_t> packet)
{
    io::BinaryReader r(packet);
    if (r.readU8() != kConfirmWalk)
    {
        return std::nullopt;
    }

    ConfirmWalk out;
    out.sequence = r.readU8();
    out.notoriety = r.readU8() & ~0x40;
    if (out.notoriety == 0 || out.notoriety >= 8)
    {
        out.notoriety = 1;
    }
    return r.overflowed() ? std::nullopt : std::optional(out);
}

std::optional<Direction> parseMovePlayer(std::span<const uint8_t> packet)
{
    io::BinaryReader r(packet);
    if (r.readU8() != kMovePlayer)
    {
        return std::nullopt;
    }

    const Direction dir = toDirection(r.readU8());
    return r.overflowed() ? std::nullopt : std::optional(dir);
}

std::optional<FastWalkKeys> parseFastWalkKeys(std::span<const uint8_t> packet)
{
    io::BinaryReader r(packet);
    if (r.readU8() != kExtendedCommand)
    {
        return std::nullopt;
    }

    r.skip(2);  // length
    const uint16_t sub = r.readU16BE();
    FastWalkKeys out;

    if (sub == kExtFastWalkKeys)
    {
        out.replace = true;
        for (int i = 0; i < 6; i++)
        {
            out.keys.push_back(r.readU32BE());
        }
    }
    else if (sub == kExtFastWalkAddKey)
    {
        out.keys.push_back(r.readU32BE());
    }
    else
    {
        return std::nullopt;
    }

    return r.overflowed() ? std::nullopt : std::optional(std::move(out));
}

}  // namespace uo::movement::packets
