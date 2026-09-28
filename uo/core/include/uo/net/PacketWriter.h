// SPDX-License-Identifier: BSD-2-Clause
// Part of the AxmolUO client core. Ported from ClassicUO.IO (StackDataWriter).
#pragma once

#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace uo::net
{

// Builds one outgoing packet. For variable-length packets call beginVariable(id) and
// finish(); the u16 length after the id is patched in at the end.
class PacketWriter
{
public:
    PacketWriter() = default;
    explicit PacketWriter(std::uint8_t id, std::size_t fixedLength = 0) : _fixed(fixedLength)
    {
        _buf.reserve(fixedLength ? fixedLength : 64);
        u8(id);
    }

    static PacketWriter variable(std::uint8_t id)
    {
        PacketWriter w(id);
        w._variable = true;
        w.u16(0);
        return w;
    }

    PacketWriter& u8(std::uint8_t v)
    {
        _buf.push_back(v);
        return *this;
    }
    PacketWriter& i8(std::int8_t v) { return u8(static_cast<std::uint8_t>(v)); }
    PacketWriter& u16(std::uint16_t v) { return u8(v >> 8).u8(v & 0xFF); }
    PacketWriter& u32(std::uint32_t v) { return u16(v >> 16).u16(v & 0xFFFF); }
    PacketWriter& u32LE(std::uint32_t v) { return u8(v & 0xFF).u8((v >> 8) & 0xFF).u8((v >> 16) & 0xFF).u8(v >> 24); }
    PacketWriter& zero(std::size_t n)
    {
        _buf.insert(_buf.end(), n, 0);
        return *this;
    }
    PacketWriter& bytes(std::span<const std::uint8_t> b)
    {
        _buf.insert(_buf.end(), b.begin(), b.end());
        return *this;
    }

    // Fixed-width ASCII, truncated or NUL-padded to `width`. width == 0 writes the text plus a NUL.
    PacketWriter& ascii(std::string_view s, std::size_t width = 0)
    {
        if (!width)
        {
            _buf.insert(_buf.end(), s.begin(), s.end());
            return u8(0);
        }
        std::size_t n = s.size() < width ? s.size() : width;
        _buf.insert(_buf.end(), s.begin(), s.begin() + n);
        return zero(width - n);
    }

    // UTF-8 in, UTF-16BE out, NUL-terminated.
    PacketWriter& unicodeBE(std::string_view utf8);

    // Pads fixed packets and patches the length of variable ones.
    std::vector<std::uint8_t> finish()
    {
        if (_variable && _buf.size() >= 3)
        {
            _buf[1] = static_cast<std::uint8_t>(_buf.size() >> 8);
            _buf[2] = static_cast<std::uint8_t>(_buf.size() & 0xFF);
        }
        else if (_fixed && _buf.size() < _fixed)
        {
            _buf.resize(_fixed, 0);
        }
        return std::move(_buf);
    }

    std::size_t size() const { return _buf.size(); }

private:
    std::vector<std::uint8_t> _buf;
    std::size_t _fixed = 0;
    bool _variable     = false;
};

}  // namespace uo::net
