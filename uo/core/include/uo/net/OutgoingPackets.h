// SPDX-License-Identifier: BSD-2-Clause
// Part of the AxmolUO client core. Ported from ClassicUO.Network (OutgoingPackets).
#pragma once

#include "uo/io/ClientVersion.h"

#include <cstdint>
#include <string_view>
#include <vector>

namespace uo::net::out
{

using Bytes = std::vector<std::uint8_t>;

// 0xEF: login seed with client version (6.0.5.0+ clients).
Bytes seed(std::uint32_t seed, ClientVersion version);
// Pre-0xEF clients (T2A included) open with a bare 4-byte seed.
Bytes seedOld(std::uint32_t seed);
// 0x80: account login to the login server.
Bytes accountLogin(std::string_view account, std::string_view password);
// 0xA0: pick a shard from the 0xA8 list.
Bytes selectShard(std::uint16_t index);
// 0x91: authenticate with the game server using the key from 0x8C.
Bytes gameLogin(std::uint32_t authKey, std::string_view account, std::string_view password);
// 0x5D: play character in `slot`.
Bytes selectCharacter(std::uint32_t slot, std::string_view name, std::uint32_t clientIp, std::uint32_t protocol);
// 0xBD: client version string, sent when the server asks for it.
Bytes clientVersion(std::string_view version);
Bytes ping(std::uint8_t sequence);
// 0x02: step in `direction` (0..7, bit 0x80 = run).
Bytes walkRequest(std::uint8_t direction, std::uint8_t sequence, std::uint32_t fastWalkKey = 0);
Bytes doubleClick(std::uint32_t serial);
Bytes singleClick(std::uint32_t serial);
Bytes resync();
// 0xAD: unicode speech.
Bytes speech(std::string_view utf8, std::uint8_t type, std::uint16_t hue, std::uint16_t font,
             std::string_view language = "ENU");
// 0x34: status/skills request.
Bytes statusRequest(std::uint32_t serial);

}  // namespace uo::net::out
