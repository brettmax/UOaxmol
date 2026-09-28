// SPDX-License-Identifier: BSD-2-Clause
// The converted map container (.uomap). See uo/tools/ASSETS.md for the byte layout.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace uoconvert
{

struct UoMapStatic
{
    std::uint8_t x = 0;  // 0..63 within the chunk
    std::uint8_t y = 0;
    std::int8_t z  = 0;
    std::uint16_t graphic = 0;
    std::uint16_t hue     = 0;
};

// One 64x64-tile chunk, row-major (index = y * 64 + x).
struct UoMapChunk
{
    static constexpr int kSize = 64;
    std::vector<std::uint16_t> land = std::vector<std::uint16_t>(kSize * kSize, 0);
    std::vector<std::int8_t> z      = std::vector<std::int8_t>(kSize * kSize, 0);
    std::vector<UoMapStatic> statics;  // sorted by (y, x), source order kept within a cell

    std::vector<std::uint8_t> serialize() const;
    bool deserialize(const std::uint8_t* data, std::size_t size);
};

struct UoMapHeader
{
    static constexpr char kMagic[4]       = {'U', 'O', 'M', 'P'};
    static constexpr std::uint16_t kVersion = 1;
    static constexpr std::uint16_t kFlagZlib = 1;
    static constexpr std::size_t kBytes      = 32;
    static constexpr std::size_t kIndexEntry = 16;

    std::uint16_t version  = kVersion;
    std::uint16_t mapIndex = 0;
    std::uint32_t width    = 0;  // tiles
    std::uint32_t height   = 0;
    std::uint16_t chunkSize = UoMapChunk::kSize;
    std::uint16_t flags     = kFlagZlib;
    std::uint32_t chunksX  = 0;
    std::uint32_t chunksY  = 0;
};

class UoMapWriter
{
public:
    bool open(const std::string& path, const UoMapHeader& header);
    // Chunks may be written in any order; each (cx, cy) exactly once.
    bool write(std::uint32_t cx, std::uint32_t cy, const UoMapChunk& chunk);
    bool finish();

private:
    std::string _path;
    UoMapHeader _h;
    std::vector<std::uint8_t> _index;
    std::vector<std::uint8_t> _body;
};

// Reference reader: what the client does, minus memory mapping. Used by the tests.
class UoMapReader
{
public:
    bool open(const std::string& path);
    const UoMapHeader& header() const { return _h; }
    bool read(std::uint32_t cx, std::uint32_t cy, UoMapChunk& out) const;

private:
    UoMapHeader _h;
    std::vector<std::uint8_t> _file;
};

}  // namespace uoconvert
