// SPDX-License-Identifier: BSD-2-Clause
// Part of the AxmolUO client core. Ported from ClassicUO.Network (PacketsTable).
#pragma once

#include "uo/io/ClientVersion.h"

#include <array>
#include <cstdint>

namespace uo::net
{

// Packet lengths the client expects for a given client version. Several packets changed
// size across eras (0x0B, 0xB9, 0xF3 ...), so the table is calibrated per version.
class PacketTable
{
public:
    explicit PacketTable(ClientVersion version);

    // Total length including the id byte, or -1 for variable-length packets.
    std::int16_t length(std::uint8_t id) const { return _lengths[id]; }
    ClientVersion version() const { return _version; }

private:
    std::array<std::int16_t, 256> _lengths{};
    ClientVersion _version;
};

}  // namespace uo::net
