// SPDX-License-Identifier: BSD-2-Clause
#include "uo/io/UOFile.h"

#include "uo/io/BinaryReader.h"
#include "uo/io/Compression.h"

#include <algorithm>
#include <cstdio>

namespace uo::io
{

const FileIndex* UOFile::entry(std::size_t index) const
{
    if (index >= _entries.size())
        return nullptr;
    const FileIndex& e = _entries[index];
    return e.valid() ? &e : nullptr;
}

std::span<const std::uint8_t> UOFile::read(std::size_t index) const
{
    const FileIndex* e = entry(index);
    return e ? raw(*e) : std::span<const std::uint8_t>{};
}

bool UOFile::readDecompressed(const FileIndex& e, std::vector<std::uint8_t>& out, bool* bwt) const
{
    auto bytes = raw(e);
    if (bwt)
        *bwt = e.compression == CompressionType::ZlibBwt;
    if (e.compression == CompressionType::None)
    {
        out.assign(bytes.begin(), bytes.end());
        return true;
    }
    return inflate(bytes, out, e.decompressed > 0 ? static_cast<std::size_t>(e.decompressed) : 0);
}

bool MulFile::load()
{
    if (!_data.open(_dataPath))
        return false;

    _entries.clear();
    if (_idxPath.empty())
        return true;

    MappedFile idx(_idxPath);
    if (!idx.isOpen())
        return false;

    BinaryReader r(idx.bytes());
    std::size_t count = idx.size() / 12;
    _entries.resize(count);

    for (std::size_t i = 0; i < count; ++i)
    {
        FileIndex& e     = _entries[i];
        std::uint32_t at = r.readU32LE();
        e.offset         = at == 0xFFFFFFFFu ? -1 : static_cast<std::int64_t>(at);
        e.length         = r.readI32LE();
        std::int32_t extra = r.readI32LE();
        if (extra > 0)
        {
            e.width  = static_cast<std::int16_t>(extra >> 16);
            e.height = static_cast<std::int16_t>(extra & 0xFFFF);
        }
    }
    return true;
}

namespace
{
constexpr std::uint32_t kUopMagic = 0x50594D;  // "MYP\0"

inline std::uint32_t rot(std::uint32_t x, int k)
{
    return (x << k) | (x >> (32 - k));
}
}  // namespace

std::uint64_t UopFile::hash(std::string_view s)
{
    // Straight transcription of lookup3's hashlittle2 as ClassicUO spells it, including its
    // register names, so the two can be diffed line by line.
    std::uint32_t eax = 0, ecx = 0, edx = 0, ebx, esi, edi;
    ebx = edi = esi = static_cast<std::uint32_t>(s.size()) + 0xDEADBEEF;

    auto at = [&](std::size_t i) { return static_cast<std::uint32_t>(static_cast<unsigned char>(s[i])); };

    std::size_t i = 0;
    for (; i + 12 < s.size(); i += 12)
    {
        edi = ((at(i + 7) << 24) | (at(i + 6) << 16) | (at(i + 5) << 8) | at(i + 4)) + edi;
        esi = ((at(i + 11) << 24) | (at(i + 10) << 16) | (at(i + 9) << 8) | at(i + 8)) + esi;
        edx = ((at(i + 3) << 24) | (at(i + 2) << 16) | (at(i + 1) << 8) | at(i)) - esi;
        edx = (edx + ebx) ^ rot(esi, 4);
        esi += edi;
        edi = (edi - edx) ^ rot(edx, 6);
        edx += esi;
        esi = (esi - edi) ^ rot(edi, 8);
        edi += edx;
        ebx = (edx - esi) ^ rot(esi, 16);
        esi += edi;
        edi = (edi - ebx) ^ rot(ebx, 19);
        ebx += esi;
        esi = (esi - edi) ^ rot(edi, 4);
        edi += ebx;
    }

    std::size_t left = s.size() - i;
    if (left > 0)
    {
        switch (left)
        {
        case 12: esi += at(i + 11) << 24; [[fallthrough]];
        case 11: esi += at(i + 10) << 16; [[fallthrough]];
        case 10: esi += at(i + 9) << 8; [[fallthrough]];
        case 9: esi += at(i + 8); [[fallthrough]];
        case 8: edi += at(i + 7) << 24; [[fallthrough]];
        case 7: edi += at(i + 6) << 16; [[fallthrough]];
        case 6: edi += at(i + 5) << 8; [[fallthrough]];
        case 5: edi += at(i + 4); [[fallthrough]];
        case 4: ebx += at(i + 3) << 24; [[fallthrough]];
        case 3: ebx += at(i + 2) << 16; [[fallthrough]];
        case 2: ebx += at(i + 1) << 8; [[fallthrough]];
        case 1: ebx += at(i); break;
        default: break;
        }

        esi = (esi ^ edi) - rot(edi, 14);
        ecx = (esi ^ ebx) - rot(esi, 11);
        edi = (edi ^ ecx) - rot(ecx, 25);
        esi = (esi ^ edi) - rot(edi, 16);
        edx = (esi ^ ecx) - rot(esi, 4);
        edi = (edi ^ edx) - rot(edx, 14);
        eax = (esi ^ edi) - rot(edi, 24);

        return (static_cast<std::uint64_t>(edi) << 32) | eax;
    }

    return (static_cast<std::uint64_t>(esi) << 32) | eax;
}

std::string UopFile::formatName(const std::string& pattern, std::uint32_t index)
{
    char buf[256];
    std::snprintf(buf, sizeof(buf), pattern.c_str(), index);
    return buf;
}

bool UopFile::load()
{
    if (!_data.open(_path))
        return false;

    BinaryReader r(_data.bytes());
    if (r.readU32LE() != kUopMagic)
        return false;

    r.readU32LE();  // version
    r.readU32LE();  // format timestamp
    std::int64_t nextBlock = r.readI64LE();
    r.readU32LE();  // block size
    r.readI32LE();  // file count

    _hashes.clear();
    std::size_t total = 0;

    while (nextBlock > 0 && static_cast<std::size_t>(nextBlock) < _data.size())
    {
        r.seek(static_cast<std::size_t>(nextBlock));
        std::int32_t filesCount = r.readI32LE();
        nextBlock               = r.readI64LE();
        total += filesCount > 0 ? filesCount : 0;

        for (std::int32_t i = 0; i < filesCount && !r.overflowed(); ++i)
        {
            std::int64_t offset       = r.readI64LE();
            std::int32_t headerLength = r.readI32LE();
            std::int32_t compressed   = r.readI32LE();
            std::int32_t decompressed = r.readI32LE();
            std::uint64_t h           = r.readU64LE();
            r.readU32LE();  // data hash
            std::int16_t flag = r.readI16LE();

            if (offset == 0)
                continue;

            offset += headerLength;

            FileIndex e;
            e.offset       = offset;
            e.length       = compressed;  // bytes on disk; `decompressed` is the inflated size
            e.decompressed = decompressed;
            e.compression  = static_cast<CompressionType>(flag);

            if (_hasExtra && flag == 0)
            {
                // Uncompressed gump entries carry width/height ahead of the pixels; compressed
                // ones carry them inside the deflated stream (see Gumps::get).
                BinaryReader extra(_data.slice(static_cast<std::size_t>(offset), 8));
                e.width  = extra.readI32LE();
                e.height = extra.readI32LE();
                e.offset += 8;
                e.length = compressed - 8;
            }

            _hashes.emplace(h, e);
        }

        if (r.overflowed())
            return false;
    }

    _entries.assign(std::max<std::size_t>(total, 0xFFFF) + 0x4000, FileIndex{-1, 0});
    for (std::size_t i = 0; i < _entries.size(); ++i)
    {
        auto it = _hashes.find(hash(formatName(_pattern, static_cast<std::uint32_t>(i))));
        if (it != _hashes.end())
            _entries[i] = it->second;
    }
    return true;
}

const FileIndex* UopFile::byHash(std::uint64_t h) const
{
    auto it = _hashes.find(h);
    return it == _hashes.end() ? nullptr : &it->second;
}

}  // namespace uo::io
