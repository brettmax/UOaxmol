// SPDX-License-Identifier: BSD-2-Clause
// Part of the AxmolUO client core. Ported from ClassicUO.Utility (ZLib).
#pragma once

#include <cstdint>
#include <span>
#include <vector>

namespace uo::io
{

// Inflates a zlib stream. `expectedSize` pre-sizes the output (UOP entries and 0xDD/0xD8
// packets carry it); pass 0 when unknown. Returns false on corrupt input.
bool inflate(std::span<const std::uint8_t> src, std::vector<std::uint8_t>& out, std::size_t expectedSize = 0);

// Deflates, for tests and for tools writing UOP files.
std::vector<std::uint8_t> deflate(std::span<const std::uint8_t> src);

}  // namespace uo::io
