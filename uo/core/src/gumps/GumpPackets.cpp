// SPDX-License-Identifier: BSD-2-Clause
#include "uo/gumps/GumpPackets.h"

#include "uo/io/BinaryReader.h"
#include "uo/io/Compression.h"
#include "uo/net/PacketWriter.h"

#include <algorithm>

namespace uo::gumps
{

namespace
{

std::optional<std::vector<uint8_t>> inflate(std::span<const uint8_t> src, size_t expected)
{
    std::vector<uint8_t> out;

    if (expected == 0)
    {
        return out;
    }

    if (!io::inflate(src, out, expected))
    {
        return std::nullopt;
    }

    return out;
}

std::span<const uint8_t> take(io::BinaryReader& r, size_t n)
{
    if (r.remaining() < n)
    {
        r.skip(n);  // marks the reader as overflowed on the next read
        return {};
    }

    auto s = r.rest().first(n);
    r.skip(n);
    return s;
}

std::string asciiUntilNul(std::span<const uint8_t> bytes)
{
    auto end = std::find(bytes.begin(), bytes.end(), uint8_t{0});
    return std::string(bytes.begin(), end);
}

// `count` records of (u16 length, UTF-16BE text). In the 0xDD text table a
// short table leaves the remaining lines empty, as ClassicUO does.
std::vector<std::string> readLines(io::BinaryReader& r, size_t count, bool lenient)
{
    std::vector<std::string> lines;
    lines.reserve(std::min<size_t>(count, 4096));

    for (size_t i = 0; i < count; ++i)
    {
        if (lenient && r.remaining() < 2)
        {
            lines.emplace_back();
            continue;
        }

        uint16_t len = r.readU16BE();

        if (!lenient && r.remaining() < size_t{len} * 2)
        {
            r.skip(size_t{len} * 2 + 1);
            break;
        }

        lines.push_back(len ? r.readUnicodeBE(len) : std::string{});
    }

    return lines;
}

bool truncated(const io::BinaryReader& r)
{
    return r.overflowed() || r.position() > r.size();
}

}  // namespace

std::u16string utf8ToUtf16(std::string_view s)
{
    std::u16string out;
    out.reserve(s.size());

    for (size_t i = 0; i < s.size();)
    {
        uint8_t b = static_cast<uint8_t>(s[i]);
        uint32_t cp;
        size_t extra;

        if (b < 0x80)
        {
            cp = b;
            extra = 0;
        }
        else if ((b & 0xE0) == 0xC0)
        {
            cp = b & 0x1F;
            extra = 1;
        }
        else if ((b & 0xF0) == 0xE0)
        {
            cp = b & 0x0F;
            extra = 2;
        }
        else if ((b & 0xF8) == 0xF0)
        {
            cp = b & 0x07;
            extra = 3;
        }
        else
        {
            out += u'�';
            ++i;
            continue;
        }

        if (i + extra >= s.size())
        {
            // Truncated sequence at the end of the string.
            out += u'�';
            break;
        }

        bool bad = false;

        for (size_t k = 1; k <= extra; ++k)
        {
            uint8_t cb = static_cast<uint8_t>(s[i + k]);

            if ((cb & 0xC0) != 0x80)
            {
                bad = true;
                break;
            }

            cp = (cp << 6) | (cb & 0x3F);
        }

        if (bad)
        {
            out += u'�';
            ++i;
            continue;
        }

        i += extra + 1;

        if (cp >= 0x10000)
        {
            cp -= 0x10000;
            out += static_cast<char16_t>(0xD800 + (cp >> 10));
            out += static_cast<char16_t>(0xDC00 + (cp & 0x3FF));
        }
        else
        {
            out += static_cast<char16_t>(cp);
        }
    }

    return out;
}

std::optional<GumpPacket> decodeOpenGump(std::span<const uint8_t> packet)
{
    io::BinaryReader r(packet);

    if (r.readU8() != 0xB0)
    {
        return std::nullopt;
    }

    r.readU16BE();  // packet length

    GumpPacket g;
    g.sender = r.readU32BE();
    g.gumpId = r.readU32BE();
    g.x = r.readI32BE();
    g.y = r.readI32BE();

    uint16_t cmdLen = r.readU16BE();
    g.layout = asciiUntilNul(take(r, cmdLen));

    uint16_t lineCount = r.readU16BE();

    if (truncated(r))
    {
        return std::nullopt;
    }

    g.lines = readLines(r, lineCount, false);

    if (truncated(r))
    {
        return std::nullopt;
    }

    return g;
}

std::optional<GumpPacket> decodeOpenCompressedGump(std::span<const uint8_t> packet)
{
    io::BinaryReader r(packet);

    if (r.readU8() != 0xDD)
    {
        return std::nullopt;
    }

    r.readU16BE();  // packet length

    GumpPacket g;
    g.sender = r.readU32BE();
    g.gumpId = r.readU32BE();
    g.x = r.readI32BE();
    g.y = r.readI32BE();

    // The compressed length on the wire includes the 4-byte decompressed length.
    uint32_t clen = r.readU32BE();
    uint32_t dlen = r.readU32BE();

    if (truncated(r) || clen < 4 || r.remaining() < clen - 4)
    {
        return std::nullopt;
    }

    auto layout = inflate(take(r, clen - 4), dlen);

    if (!layout)
    {
        return std::nullopt;
    }

    g.layout = asciiUntilNul(*layout);

    uint32_t lineCount = r.readU32BE();

    if (truncated(r))
    {
        return std::nullopt;
    }

    if (lineCount != 0)
    {
        clen = r.readU32BE();
        dlen = r.readU32BE();

        if (truncated(r) || clen < 4 || r.remaining() < clen - 4)
        {
            return std::nullopt;
        }

        auto text = inflate(take(r, clen - 4), dlen);

        if (!text)
        {
            return std::nullopt;
        }

        io::BinaryReader tr(*text);
        g.lines = readLines(tr, lineCount, true);
        g.lines.resize(lineCount);
    }

    return g;
}

std::optional<GumpLayout> decodeGump(std::span<const uint8_t> packet)
{
    if (packet.empty())
    {
        return std::nullopt;
    }

    std::optional<GumpPacket> p;

    switch (packet[0])
    {
    case 0xB0:
        p = decodeOpenGump(packet);
        break;
    case 0xDD:
        p = decodeOpenCompressedGump(packet);
        break;
    default:
        return std::nullopt;
    }

    if (!p)
    {
        return std::nullopt;
    }

    GumpLayout layout = parseLayout(p->layout, p->lines);
    layout.sender = p->sender;
    layout.gumpId = p->gumpId;
    layout.x = p->x;
    layout.y = p->y;
    return layout;
}

std::vector<uint8_t> encodeGumpResponse(uint32_t sender, uint32_t gumpId, uint32_t buttonId,
                                        std::span<const uint32_t> switches,
                                        std::span<const std::pair<uint16_t, std::string>> entries)
{
    auto w = net::PacketWriter::variable(0xB1);
    w.u32(sender).u32(gumpId).u32(buttonId);
    w.u32(static_cast<uint32_t>(switches.size()));

    for (uint32_t s : switches)
    {
        w.u32(s);
    }

    w.u32(static_cast<uint32_t>(entries.size()));

    for (const auto& [id, text] : entries)
    {
        std::u16string u = utf8ToUtf16(text);
        size_t len = std::min<size_t>(239, u.size());

        w.u16(id).u16(static_cast<uint16_t>(len));

        for (size_t i = 0; i < len; ++i)
        {
            w.u16(static_cast<uint16_t>(u[i]));
        }
    }

    return w.finish();
}

}  // namespace uo::gumps
