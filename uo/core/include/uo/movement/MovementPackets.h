// SPDX-License-Identifier: BSD-2-Clause
// Incoming movement packets (ClassicUO Network/PacketHandlers.cs). Parsers take the whole packet,
// id byte included, as PacketFramer delivers it. The requests the client sends (0x02 walk, 0x22
// resync) are built by uo::net::out.

#pragma once

#include "uo/movement/Direction.h"

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace uo::movement::packets
{

inline constexpr uint8_t kWalkRequest    = 0x02;
inline constexpr uint8_t kDenyWalk       = 0x21;
inline constexpr uint8_t kConfirmWalk    = 0x22;  // also the client's resync request
inline constexpr uint8_t kMovePlayer     = 0x97;
inline constexpr uint8_t kExtendedCommand = 0xBF;

inline constexpr uint16_t kExtFastWalkKeys  = 0x0001;
inline constexpr uint16_t kExtFastWalkAddKey = 0x0002;

struct DenyWalk
{
    uint8_t sequence;
    uint16_t x, y;
    Direction direction;  // masked to the low three bits
    int8_t z;
};
std::optional<DenyWalk> parseDenyWalk(std::span<const uint8_t> packet);

struct ConfirmWalk
{
    uint8_t sequence;
    uint8_t notoriety;  // 1..7; 0 and out-of-range values become 1, as in ClassicUO
};
std::optional<ConfirmWalk> parseConfirmWalk(std::span<const uint8_t> packet);

// 0x97: the server moves the player one step (direction | 0x80 running).
std::optional<Direction> parseMovePlayer(std::span<const uint8_t> packet);

// 0xBF subcommand 1 (replace the stack; six keys) or 2 (push one key).
struct FastWalkKeys
{
    bool replace{false};
    std::vector<uint32_t> keys;
};
std::optional<FastWalkKeys> parseFastWalkKeys(std::span<const uint8_t> packet);

}  // namespace uo::movement::packets
