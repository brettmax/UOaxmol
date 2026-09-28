// SPDX-License-Identifier: BSD-2-Clause
#pragma once

#include <cstdint>
#include <string>

namespace uoconvert
{

// Writes width * height RGBA8 pixels (uocore's byte order: R, G, B, A in memory) as a PNG,
// deflated with uocore's zlib.
bool writePng(const std::string& path, int width, int height, const std::uint32_t* rgba);

}  // namespace uoconvert
