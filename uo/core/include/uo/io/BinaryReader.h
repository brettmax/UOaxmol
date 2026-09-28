// SPDX-License-Identifier: BSD-2-Clause
// Part of the AxmolUO client core. Ported from ClassicUO.IO (StackDataReader).
#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <string>

namespace uo::io
{

// Cursor over a byte span. UO files are little-endian; the network protocol is big-endian,
// so both flavours are here. Reads past the end yield zero and set overflowed() instead of
// throwing: a truncated packet or file entry must never take the client down.
class BinaryReader
{
public:
    BinaryReader() = default;
    explicit BinaryReader(std::span<const std::uint8_t> data) : _data(data) {}

    std::size_t position() const { return _pos; }
    std::size_t size() const { return _data.size(); }
    std::size_t remaining() const { return _pos < _data.size() ? _data.size() - _pos : 0; }
    bool overflowed() const { return _overflow; }
    std::span<const std::uint8_t> buffer() const { return _data; }
    std::span<const std::uint8_t> rest() const { return _data.subspan(_pos < _data.size() ? _pos : _data.size()); }

    void seek(std::size_t pos) { _pos = pos; }
    void skip(std::size_t n) { _pos += n; }

    std::uint8_t readU8()
    {
        if (_pos + 1 > _data.size())
            return fail<std::uint8_t>(1);
        return _data[_pos++];
    }
    std::int8_t readI8() { return static_cast<std::int8_t>(readU8()); }

    std::uint16_t readU16LE() { return static_cast<std::uint16_t>(readLE(2)); }
    std::uint32_t readU32LE() { return static_cast<std::uint32_t>(readLE(4)); }
    std::uint64_t readU64LE() { return readLE(8); }
    std::int16_t readI16LE() { return static_cast<std::int16_t>(readU16LE()); }
    std::int32_t readI32LE() { return static_cast<std::int32_t>(readU32LE()); }
    std::int64_t readI64LE() { return static_cast<std::int64_t>(readU64LE()); }

    std::uint16_t readU16BE() { return static_cast<std::uint16_t>(readBE(2)); }
    std::uint32_t readU32BE() { return static_cast<std::uint32_t>(readBE(4)); }
    std::int16_t readI16BE() { return static_cast<std::int16_t>(readU16BE()); }
    std::int32_t readI32BE() { return static_cast<std::int32_t>(readU32BE()); }

    bool read(std::span<std::uint8_t> out)
    {
        if (_pos + out.size() > _data.size())
        {
            std::memset(out.data(), 0, out.size());
            fail<int>(out.size());
            return false;
        }
        std::memcpy(out.data(), _data.data() + _pos, out.size());
        _pos += out.size();
        return true;
    }

    // Fixed-width, NUL-padded ASCII field (account names, shard names). A width of zero means
    // "until NUL or end of buffer".
    std::string readASCII(std::size_t width = 0)
    {
        std::string out;
        std::size_t end = width ? _pos + width : _data.size();
        std::size_t i   = _pos;
        for (; i < end && i < _data.size(); ++i)
        {
            char c = static_cast<char>(_data[i]);
            if (c == '\0')
            {
                if (!width)
                {
                    ++i;
                    break;
                }
                // fixed width: keep consuming but stop appending
                i = end;
                break;
            }
            out.push_back(c);
        }
        if (width && end > _data.size())
            _overflow = true;
        _pos = width ? end : i;
        return out;
    }

    // UTF-16BE string of `chars` code units, or NUL-terminated when chars == 0. Returned as UTF-8.
    std::string readUnicodeBE(std::size_t chars = 0);
    // UTF-16LE variant, used by the cliloc-argument packets.
    std::string readUnicodeLE(std::size_t chars = 0);

private:
    template <typename T>
    T fail(std::size_t n)
    {
        _overflow = true;
        _pos += n;
        return T{};
    }

    std::uint64_t readLE(int n)
    {
        if (_pos + n > _data.size())
            return fail<std::uint64_t>(n);
        std::uint64_t v = 0;
        for (int i = 0; i < n; ++i)
            v |= static_cast<std::uint64_t>(_data[_pos + i]) << (8 * i);
        _pos += n;
        return v;
    }

    std::uint64_t readBE(int n)
    {
        if (_pos + n > _data.size())
            return fail<std::uint64_t>(n);
        std::uint64_t v = 0;
        for (int i = 0; i < n; ++i)
            v = (v << 8) | _data[_pos + i];
        _pos += n;
        return v;
    }

    std::span<const std::uint8_t> _data;
    std::size_t _pos = 0;
    bool _overflow   = false;
};

// Appends a UTF-16 code unit sequence to a UTF-8 string, pairing surrogates.
void appendUtf16AsUtf8(std::string& out, const std::uint16_t* units, std::size_t count);

}  // namespace uo::io
