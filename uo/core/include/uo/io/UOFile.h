// SPDX-License-Identifier: BSD-2-Clause
// Part of the AxmolUO client core. Ported from ClassicUO.IO (UOFile, UOFileMul, UOFileUop,
// UOFileIndex).
#pragma once

#include "uo/io/MappedFile.h"

#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

namespace uo::io
{

enum class CompressionType : std::uint16_t
{
    None    = 0,
    Zlib    = 1,
    ZlibBwt = 3,
};

// One addressable asset inside a .mul or .uop archive.
struct FileIndex
{
    std::int64_t offset         = 0;
    std::int32_t length         = 0;
    std::int32_t decompressed   = 0;
    CompressionType compression = CompressionType::None;
    std::int32_t width          = 0;  // gump width / "extra" for .mul idx entries
    std::int32_t height         = 0;
    std::uint16_t hue           = 0;

    bool valid() const { return offset >= 0 && length > 0 && offset != 0xFFFFFFFFll; }
};

// An indexed UO data archive. Both flavours expose the same "entry N -> byte range" view so
// asset loaders never care whether the installation is a classic MUL one or a UOP one.
class UOFile
{
public:
    virtual ~UOFile() = default;

    // Reads the index. Returns false when the archive is missing or malformed.
    virtual bool load() = 0;

    const std::vector<FileIndex>& entries() const { return _entries; }
    std::size_t count() const { return _entries.size(); }

    // The entry, or nullptr when out of range or not present in this archive.
    const FileIndex* entry(std::size_t index) const;

    // Raw (possibly compressed) bytes of an entry.
    std::span<const std::uint8_t> raw(const FileIndex& e) const
    {
        return _data.slice(static_cast<std::size_t>(e.offset), static_cast<std::size_t>(e.length));
    }

    // Raw bytes of entry `index`; empty when absent.
    std::span<const std::uint8_t> read(std::size_t index) const;

    // Entry bytes with zlib compression undone. BWT-wrapped entries (7.0.100+) come back
    // inflated but still BWT-encoded; `bwt` is set so the caller can finish decoding.
    bool readDecompressed(const FileIndex& e, std::vector<std::uint8_t>& out, bool* bwt = nullptr) const;

    const MappedFile& data() const { return _data; }

protected:
    MappedFile _data;
    std::vector<FileIndex> _entries;
};

// Classic archive: foo.mul holds data, fooidx.mul (or the same file for idx-less formats
// such as map0.mul) holds 12-byte records { u32 offset, i32 length, i32 extra }.
class MulFile final : public UOFile
{
public:
    MulFile(std::string dataPath, std::string idxPath) : _dataPath(std::move(dataPath)), _idxPath(std::move(idxPath)) {}

    bool load() override;

private:
    std::string _dataPath;
    std::string _idxPath;
};

// Mythic Package (.uop). Entries are looked up by the hash of their virtual file name, built
// from `pattern` with the index substituted (e.g. "build/artlegacymul/%08u.tga").
class UopFile final : public UOFile
{
public:
    UopFile(std::string path, std::string pattern, bool hasExtra = false)
        : _path(std::move(path)), _pattern(std::move(pattern)), _hasExtra(hasExtra)
    {}

    bool load() override;

    // Lookup by raw virtual-path hash, for callers that address entries by name.
    const FileIndex* byHash(std::uint64_t hash) const;

    // Mythic's variant of Bob Jenkins' lookup3 hashlittle2, returning (b << 32) | c.
    static std::uint64_t hash(std::string_view s);

    // Expands a printf-style pattern with a single unsigned argument.
    static std::string formatName(const std::string& pattern, std::uint32_t index);

private:
    std::string _path;
    std::string _pattern;
    bool _hasExtra;
    std::unordered_map<std::uint64_t, FileIndex> _hashes;
};

}  // namespace uo::io
