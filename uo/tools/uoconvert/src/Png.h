// SPDX-License-Identifier: BSD-2-Clause
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace uoconvert
{

// Writes width * height RGBA8 pixels (uocore's byte order: R, G, B, A in memory) as a PNG.
bool writePng(const std::string& path, int width, int height, const std::uint32_t* rgba);

// zlib deflate at the given level (0-9), used for PNGs and .uomap chunks.
std::vector<std::uint8_t> deflate(const std::uint8_t* data, std::size_t size, int level);

void setPngCompression(int level);

}  // namespace uoconvert
