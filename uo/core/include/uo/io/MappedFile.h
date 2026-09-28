// SPDX-License-Identifier: BSD-2-Clause
// Part of the AxmolUO client core. Ported from ClassicUO.IO (MMFileReader).
#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>

namespace uo::io
{

// Read-only memory mapping of a whole file. UO data files (map0.mul, art.mul, the .uop
// archives) run to hundreds of megabytes and are read at random offsets, so they are mapped
// rather than streamed, exactly as ClassicUO does.
class MappedFile
{
public:
    MappedFile() = default;
    explicit MappedFile(const std::string& path);
    ~MappedFile();

    MappedFile(const MappedFile&)            = delete;
    MappedFile& operator=(const MappedFile&) = delete;
    MappedFile(MappedFile&& other) noexcept;
    MappedFile& operator=(MappedFile&& other) noexcept;

    bool open(const std::string& path);
    void close();

    bool isOpen() const { return _data != nullptr || (_open && _size == 0); }
    const std::string& path() const { return _path; }
    std::size_t size() const { return _size; }
    const std::uint8_t* data() const { return _data; }

    // Bytes [offset, offset + length), clamped to the file. Empty when the range starts
    // outside the file, so a bad index entry reads as "missing" instead of crashing.
    std::span<const std::uint8_t> slice(std::size_t offset, std::size_t length) const;
    std::span<const std::uint8_t> bytes() const { return {_data, _size}; }

private:
    std::string _path;
    const std::uint8_t* _data = nullptr;
    std::size_t _size         = 0;
    bool _open                = false;
#if defined(_WIN32)
    void* _file    = nullptr;
    void* _mapping = nullptr;
#endif
};

}  // namespace uo::io
