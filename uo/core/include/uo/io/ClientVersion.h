// SPDX-License-Identifier: BSD-2-Clause
// Part of the AxmolUO client core. Ported from ClassicUO.Utility (ClientVersion).
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace uo
{

// Client versions are packed as (major << 24) | (minor << 16) | (build << 8) | revision so
// they compare with plain integer operators. A letter revision ("4.0.11d") packs as the letter.
using ClientVersion = std::uint32_t;

constexpr ClientVersion makeVersion(std::uint32_t a, std::uint32_t b, std::uint32_t c, std::uint32_t d)
{
    return (a << 24) | (b << 16) | (c << 8) | (d & 0xFF);
}

namespace cv
{
constexpr ClientVersion CV_200     = makeVersion(2, 0, 0, 0);  // The Second Age
constexpr ClientVersion CV_4011D   = makeVersion(4, 0, 11, 'd');
constexpr ClientVersion CV_500A    = makeVersion(5, 0, 0, 'a');
constexpr ClientVersion CV_5090    = makeVersion(5, 0, 9, 0);
constexpr ClientVersion CV_6013    = makeVersion(6, 0, 1, 3);
constexpr ClientVersion CV_6017    = makeVersion(6, 0, 1, 8);
constexpr ClientVersion CV_6060    = makeVersion(6, 0, 6, 0);
constexpr ClientVersion CV_60142   = makeVersion(6, 0, 14, 2);
constexpr ClientVersion CV_7000    = makeVersion(7, 0, 0, 0);
constexpr ClientVersion CV_7090    = makeVersion(7, 0, 9, 0);  // High Seas: 64-bit tile flags
constexpr ClientVersion CV_70180   = makeVersion(7, 0, 18, 0);
constexpr ClientVersion CV_70331   = makeVersion(7, 0, 33, 1);
constexpr ClientVersion CV_706400  = makeVersion(7, 0, 64, 0);
constexpr ClientVersion CV_7010400 = makeVersion(7, 0, 104, 0);
}  // namespace cv

// Parses "7.0.15.1", "4.0.11d" or "2.0.0". Returns nullopt for anything else.
std::optional<ClientVersion> parseClientVersion(std::string_view text);
std::string clientVersionToString(ClientVersion v);

}  // namespace uo
