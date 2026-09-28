// SPDX-License-Identifier: BSD-2-Clause
// Ported from ClassicUO (BwtDecompress.cs).

#pragma once

#include <cstdint>
#include <span>
#include <vector>

namespace uo::assets
{

// Newer clients ship Cliloc.* compressed with a move-to-front + inverse BWT scheme.
// Such files have 0x8E at byte 3. Returns an empty vector on malformed input.
std::vector<uint8_t> bwtDecompress(std::span<const uint8_t> buffer);

inline bool isBwtCompressed(std::span<const uint8_t> buffer)
{
    return buffer.size() > 3 && buffer[3] == 0x8E;
}

}  // namespace uo::assets
