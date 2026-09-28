// SPDX-License-Identifier: BSD-2-Clause
// Ported from ClassicUO (ClassicUO.Utility/BwtDecompress.cs): the move-to-front + BWT
// stage applied after zlib to UOP entries flagged CompressionType::ZlibBwt.
#pragma once

#include <cstdint>
#include <span>
#include <vector>

namespace uo::io
{

// Returns an empty vector when the input is malformed.
std::vector<uint8_t> bwtDecompress(std::span<const uint8_t> buffer);

} // namespace uo::io
