// SPDX-License-Identifier: BSD-2-Clause
#include "uo/io/BinaryReader.h"

#include <vector>

namespace uo::io
{

void appendUtf16AsUtf8(std::string& out, const std::uint16_t* units, std::size_t count)
{
    for (std::size_t i = 0; i < count; ++i)
    {
        std::uint32_t cp = units[i];
        if (cp >= 0xD800 && cp <= 0xDBFF && i + 1 < count && units[i + 1] >= 0xDC00 && units[i + 1] <= 0xDFFF)
        {
            cp = 0x10000 + ((cp - 0xD800) << 10) + (units[i + 1] - 0xDC00);
            ++i;
        }

        if (cp < 0x80)
        {
            out.push_back(static_cast<char>(cp));
        }
        else if (cp < 0x800)
        {
            out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        }
        else if (cp < 0x10000)
        {
            out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        }
        else
        {
            out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        }
    }
}

namespace
{
template <typename ReadUnit>
std::string readUtf16(BinaryReader& r, std::size_t chars, ReadUnit readUnit)
{
    std::vector<std::uint16_t> units;
    if (chars)
    {
        units.reserve(chars);
        bool ended = false;
        for (std::size_t i = 0; i < chars; ++i)
        {
            std::uint16_t u = readUnit();
            if (u == 0)
                ended = true;
            if (!ended)
                units.push_back(u);
        }
    }
    else
    {
        while (r.remaining() >= 2)
        {
            std::uint16_t u = readUnit();
            if (u == 0)
                break;
            units.push_back(u);
        }
    }
    std::string out;
    appendUtf16AsUtf8(out, units.data(), units.size());
    return out;
}
}  // namespace

std::string BinaryReader::cp1252ToUtf8(std::string_view bytes)
{
    // 0x80-0x9F differ from Latin-1; unassigned code points map to themselves, like .NET.
    static constexpr std::uint16_t kHigh[32] = {
        0x20AC, 0x0081, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021, 0x02C6, 0x2030, 0x0160,
        0x2039, 0x0152, 0x008D, 0x017D, 0x008F, 0x0090, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022,
        0x2013, 0x2014, 0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0x009D, 0x017E, 0x0178,
    };

    std::string out;
    out.reserve(bytes.size());
    for (char c : bytes)
    {
        const auto b = static_cast<std::uint8_t>(c);
        if (b < 0x80)
        {
            out.push_back(c);
            continue;
        }
        const std::uint16_t unit = b < 0xA0 ? kHigh[b - 0x80] : b;
        appendUtf16AsUtf8(out, &unit, 1);
    }
    return out;
}

std::string BinaryReader::readUnicodeBE(std::size_t chars)
{
    return readUtf16(*this, chars, [this] { return readU16BE(); });
}

std::string BinaryReader::readUnicodeLE(std::size_t chars)
{
    return readUtf16(*this, chars, [this] { return readU16LE(); });
}

}  // namespace uo::io
