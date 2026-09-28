// SPDX-License-Identifier: BSD-2-Clause
#include "UoMap.h"

#include "Png.h"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <zlib.h>

namespace uoconvert
{

namespace
{
void put16(std::vector<std::uint8_t>& b, std::uint16_t v)
{
    b.push_back(v & 0xFF);
    b.push_back(v >> 8);
}
void put32(std::vector<std::uint8_t>& b, std::uint32_t v)
{
    put16(b, v & 0xFFFF);
    put16(b, v >> 16);
}
void put64(std::vector<std::uint8_t>& b, std::uint64_t v)
{
    put32(b, static_cast<std::uint32_t>(v));
    put32(b, static_cast<std::uint32_t>(v >> 32));
}
std::uint16_t get16(const std::uint8_t* p)
{
    return static_cast<std::uint16_t>(p[0] | (p[1] << 8));
}
std::uint32_t get32(const std::uint8_t* p)
{
    return get16(p) | (static_cast<std::uint32_t>(get16(p + 2)) << 16);
}
std::uint64_t get64(const std::uint8_t* p)
{
    return get32(p) | (static_cast<std::uint64_t>(get32(p + 4)) << 32);
}
constexpr std::size_t kCells = UoMapChunk::kSize * UoMapChunk::kSize;
constexpr std::size_t kStaticBytes = 8;
}  // namespace

std::vector<std::uint8_t> UoMapChunk::serialize() const
{
    std::vector<std::uint8_t> b;
    b.reserve(kCells * 3 + 4 + statics.size() * kStaticBytes);
    for (std::uint16_t t : land)
        put16(b, t);
    for (std::int8_t v : z)
        b.push_back(static_cast<std::uint8_t>(v));
    put32(b, static_cast<std::uint32_t>(statics.size()));
    for (const auto& s : statics)
    {
        b.push_back(s.x);
        b.push_back(s.y);
        b.push_back(static_cast<std::uint8_t>(s.z));
        b.push_back(0);
        put16(b, s.graphic);
        put16(b, s.hue);
    }
    return b;
}

bool UoMapChunk::deserialize(const std::uint8_t* d, std::size_t size)
{
    if (size < kCells * 3 + 4)
        return false;
    for (std::size_t i = 0; i < kCells; ++i)
        land[i] = get16(d + i * 2);
    for (std::size_t i = 0; i < kCells; ++i)
        z[i] = static_cast<std::int8_t>(d[kCells * 2 + i]);
    std::uint32_t n = get32(d + kCells * 3);
    const std::uint8_t* p = d + kCells * 3 + 4;
    if (size < kCells * 3 + 4 + static_cast<std::size_t>(n) * kStaticBytes)
        return false;
    statics.resize(n);
    for (auto& s : statics)
    {
        s.x = p[0];
        s.y = p[1];
        s.z = static_cast<std::int8_t>(p[2]);
        s.graphic = get16(p + 4);
        s.hue     = get16(p + 6);
        p += kStaticBytes;
    }
    return true;
}

bool UoMapWriter::open(const std::string& path, const UoMapHeader& header, int zlibLevel)
{
    _path  = path;
    _h     = header;
    _level = zlibLevel;
    _index.assign(static_cast<std::size_t>(_h.chunksX) * _h.chunksY * UoMapHeader::kIndexEntry, 0);
    _body.clear();
    return true;
}

bool UoMapWriter::write(std::uint32_t cx, std::uint32_t cy, const UoMapChunk& chunk)
{
    if (cx >= _h.chunksX || cy >= _h.chunksY)
        return false;
    std::vector<std::uint8_t> raw = chunk.serialize();
    std::vector<std::uint8_t> stored =
        (_h.flags & UoMapHeader::kFlagZlib) ? deflate(raw.data(), raw.size(), _level) : raw;
    std::size_t at = (static_cast<std::size_t>(cy) * _h.chunksX + cx) * UoMapHeader::kIndexEntry;
    std::vector<std::uint8_t> e;
    put64(e, _body.size());  // relative to the body; fixed up in finish()
    put32(e, static_cast<std::uint32_t>(stored.size()));
    put32(e, static_cast<std::uint32_t>(raw.size()));
    std::copy(e.begin(), e.end(), _index.begin() + at);
    _body.insert(_body.end(), stored.begin(), stored.end());
    return true;
}

bool UoMapWriter::finish()
{
    std::vector<std::uint8_t> head;
    head.insert(head.end(), UoMapHeader::kMagic, UoMapHeader::kMagic + 4);
    put16(head, _h.version);
    put16(head, _h.mapIndex);
    put32(head, _h.width);
    put32(head, _h.height);
    put16(head, _h.chunkSize);
    put16(head, _h.flags);
    put32(head, _h.chunksX);
    put32(head, _h.chunksY);
    put32(head, 0);  // reserved

    const std::uint64_t bodyStart = UoMapHeader::kBytes + _index.size();
    for (std::size_t at = 0; at < _index.size(); at += UoMapHeader::kIndexEntry)
    {
        std::uint64_t rel = get64(&_index[at]);
        std::uint32_t stored = get32(&_index[at + 8]);
        std::vector<std::uint8_t> fixed;
        put64(fixed, stored ? rel + bodyStart : 0);
        std::copy(fixed.begin(), fixed.end(), _index.begin() + at);
    }

    std::ofstream f(_path, std::ios::binary);
    f.write(reinterpret_cast<const char*>(head.data()), static_cast<std::streamsize>(head.size()));
    f.write(reinterpret_cast<const char*>(_index.data()), static_cast<std::streamsize>(_index.size()));
    f.write(reinterpret_cast<const char*>(_body.data()), static_cast<std::streamsize>(_body.size()));
    _body.clear();
    _body.shrink_to_fit();
    return static_cast<bool>(f);
}

bool UoMapReader::open(const std::string& path)
{
    std::ifstream f(path, std::ios::binary);
    _file.assign(std::istreambuf_iterator<char>(f), {});
    if (_file.size() < UoMapHeader::kBytes || std::memcmp(_file.data(), UoMapHeader::kMagic, 4) != 0)
        return false;
    const std::uint8_t* p = _file.data();
    _h.version   = get16(p + 4);
    _h.mapIndex  = get16(p + 6);
    _h.width     = get32(p + 8);
    _h.height    = get32(p + 12);
    _h.chunkSize = get16(p + 16);
    _h.flags     = get16(p + 18);
    _h.chunksX   = get32(p + 20);
    _h.chunksY   = get32(p + 24);
    return _h.version == UoMapHeader::kVersion && _h.chunkSize == UoMapChunk::kSize &&
           _file.size() >= UoMapHeader::kBytes + static_cast<std::size_t>(_h.chunksX) * _h.chunksY * 16;
}

bool UoMapReader::read(std::uint32_t cx, std::uint32_t cy, UoMapChunk& out) const
{
    if (cx >= _h.chunksX || cy >= _h.chunksY)
        return false;
    const std::uint8_t* e = _file.data() + UoMapHeader::kBytes + (static_cast<std::size_t>(cy) * _h.chunksX + cx) * 16;
    std::uint64_t offset = get64(e);
    std::uint32_t stored = get32(e + 8);
    std::uint32_t rawSize = get32(e + 12);
    if (!offset || offset + stored > _file.size())
        return false;
    if (!(_h.flags & UoMapHeader::kFlagZlib))
        return out.deserialize(_file.data() + offset, stored);
    std::vector<std::uint8_t> raw(rawSize);
    uLongf size = rawSize;
    if (uncompress(raw.data(), &size, _file.data() + offset, stored) != Z_OK || size != rawSize)
        return false;
    return out.deserialize(raw.data(), raw.size());
}

}  // namespace uoconvert
