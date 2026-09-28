// SPDX-License-Identifier: BSD-2-Clause
// Part of the AxmolUO client core. Ported from ClassicUO.Assets (ClilocLoader).

#include "uo/assets/Cliloc.h"

#include "uo/assets/Bwt.h"
#include "uo/assets/Installation.h"
#include "uo/io/MappedFile.h"
#include "uo/text/Utf.h"

#include <algorithm>
#include <charconv>
#include <cstring>
#include <optional>
#include <vector>

namespace uo::assets
{

namespace
{

bool isAsciiSpace(char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

// int.TryParse with NumberStyles.Integer: optional surrounding white space and a
// leading sign, decimal digits, must fit in 32 bits.
std::optional<int> parseInt(std::string_view s)
{
    while (!s.empty() && isAsciiSpace(s.front()))
        s.remove_prefix(1);
    while (!s.empty() && isAsciiSpace(s.back()))
        s.remove_suffix(1);

    if (!s.empty() && s.front() == '+')
        s.remove_prefix(1);

    if (s.empty())
        return std::nullopt;

    int value     = 0;
    auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), value);

    if (ec != std::errc{} || ptr != s.data() + s.size())
        return std::nullopt;

    return value;
}

int32_t readI32(const uint8_t* p)
{
    return static_cast<int32_t>(static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
                                (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24));
}

int16_t readI16(const uint8_t* p)
{
    return static_cast<int16_t>(static_cast<uint16_t>(p[0] | (p[1] << 8)));
}

bool equalsIgnoreCase(std::string_view a, std::string_view b)
{
    if (a.size() != b.size())
        return false;

    for (size_t i = 0; i < a.size(); ++i)
    {
        const char x = (a[i] >= 'A' && a[i] <= 'Z') ? static_cast<char>(a[i] + 32) : a[i];
        const char y = (b[i] >= 'A' && b[i] <= 'Z') ? static_cast<char>(b[i] + 32) : b[i];

        if (x != y)
            return false;
    }

    return true;
}

std::string missingText(int number)
{
    return "MegaCliloc: missing " + std::to_string(number) + " [~1_val~] [~2_val~]";
}

}  // namespace

bool Cliloc::load(const std::string& path)
{
    io::MappedFile file(path);
    return file.isOpen() && loadFromBytes(file.bytes());
}

bool Cliloc::load(const Installation& installation, std::string_view lang)
{
    if (lang.empty())
        lang = "enu";

    std::string name = "Cliloc." + std::string(lang);

    if (!installation.exists(name))
        name = "Cliloc.enu";

    bool ok = false;

    if (installation.exists(name))
    {
        io::MappedFile file(installation.path(name));

        if (!file.isOpen())
            return false;

        if (!equalsIgnoreCase(name, "cliloc.enu"))
        {
            io::MappedFile enu(installation.path("Cliloc.enu"));

            if (enu.isOpen())
                loadFromBytes(enu.bytes());
        }

        ok = loadFromBytes(file.bytes());
    }
    else
    {
        // 1.x clients predate Cliloc.enu: their system messages are in cliloc-1.<lang>.
        name = "cliloc-1." + std::string(lang);

        if (!installation.exists(name))
            name = "cliloc-1.enu";

        io::MappedFile file(installation.path(name));

        if (!file.isOpen() || !loadLegacyFromBytes(file.bytes(), 500000))
            return false;

        ok = true;
    }

    io::MappedFile ours(installation.path("Clilocs.txt"));

    if (ours.isOpen())
        loadOverrides(std::string_view(reinterpret_cast<const char*>(ours.data()), ours.size()));

    return ok;
}

bool Cliloc::loadFromBytes(std::span<const uint8_t> data)
{
    std::vector<uint8_t> decompressed;

    if (isBwtCompressed(data))
    {
        decompressed = bwtDecompress(data);
        if (decompressed.empty())
            return false;
        data = decompressed;
    }

    if (data.size() < 6)
        return false;

    size_t pos = 6;  // int32 header + int16 header

    while (pos < data.size())
    {
        if (pos + 7 > data.size())
            break;

        const int number     = readI32(data.data() + pos);
        const int16_t length = readI16(data.data() + pos + 5);
        pos += 7;

        if (length < 0)
            break;

        size_t size = static_cast<size_t>(length);
        if (size > data.size() - pos)
            size = data.size() - pos;

        // The original reader stops a fixed-length string at the first NUL.
        const auto* begin = reinterpret_cast<const char*>(data.data() + pos);
        const auto* nul   = static_cast<const char*>(std::memchr(begin, 0, size));
        const size_t textLen = nul ? static_cast<size_t>(nul - begin) : size;

        _entries[number].assign(begin, textLen);
        pos += size;
    }

    return true;
}

bool Cliloc::loadLegacyFromBytes(std::span<const uint8_t> data, std::int32_t base)
{
    // IFF chunks: 4-byte tag, big-endian u32 size, payload padded to an even length. A FORM
    // payload starts with its own 4-byte type, then nested chunks.
    auto readU32Be = [](const uint8_t* p) {
        return (static_cast<uint32_t>(p[0]) << 24) | (static_cast<uint32_t>(p[1]) << 16) |
               (static_cast<uint32_t>(p[2]) << 8) | static_cast<uint32_t>(p[3]);
    };

    std::span<const uint8_t> text;
    // INFO: 4-byte language, then a little-endian u32 encoding: 1 = single-byte Latin-1
    // (enu, deu, fra), 2 = UTF-16LE (cht, jpn, kor).
    bool utf16 = false;
    size_t pos = 0;
    size_t end = data.size();

    while (pos + 8 <= end)
    {
        const auto* tag   = data.data() + pos;
        const size_t size = std::min<size_t>(readU32Be(tag + 4), end - pos - 8);

        if (std::memcmp(tag, "FORM", 4) == 0 && size >= 4)
        {
            // Descend: DATA and LANG forms only nest the chunks we want.
            end = pos + 8 + size;
            pos += 12;
            continue;
        }

        if (std::memcmp(tag, "INFO", 4) == 0 && size >= 8)
        {
            utf16 = readI32(tag + 12) == 2;
        }
        else if (std::memcmp(tag, "TEXT", 4) == 0)
        {
            text = data.subspan(pos + 8, size);
            break;
        }

        pos += 8 + size + (size & 1);
    }

    if (text.empty())
        return false;

    std::int32_t number = base;

    if (utf16)
    {
        std::u16string current;

        for (size_t i = 0; i + 1 < text.size(); i += 2)
        {
            const char16_t c = static_cast<char16_t>(text[i] | (text[i + 1] << 8));

            if (c != 0)
            {
                current += c;
                continue;
            }

            _entries[number++] = text::utf16ToUtf8(current);
            current.clear();
        }

        return true;
    }

    size_t start = 0;

    for (size_t i = 0; i < text.size(); ++i)
    {
        if (text[i] != 0)
            continue;

        std::string utf8;
        utf8.reserve(i - start);

        for (size_t k = start; k < i; ++k)
        {
            const uint8_t c = text[k];

            if (c < 0x80)
            {
                utf8 += static_cast<char>(c);
            }
            else
            {
                utf8 += static_cast<char>(0xC0 | (c >> 6));
                utf8 += static_cast<char>(0x80 | (c & 0x3F));
            }
        }

        _entries[number++] = std::move(utf8);
        start = i + 1;
    }

    return true;
}

int Cliloc::loadOverrides(std::string_view text)
{
    int added = 0;

    // Skip a UTF-8 byte order mark, which File.ReadLines also ignores.
    if (text.size() >= 3 && text.substr(0, 3) == "\xEF\xBB\xBF")
        text.remove_prefix(3);

    while (!text.empty())
    {
        size_t eol           = text.find('\n');
        std::string_view line = text.substr(0, eol);
        text                  = eol == std::string_view::npos ? std::string_view{} : text.substr(eol + 1);

        while (!line.empty() && isAsciiSpace(line.front()))
            line.remove_prefix(1);
        while (!line.empty() && isAsciiSpace(line.back()))
            line.remove_suffix(1);

        if (line.empty() || line.front() == '#')
            continue;

        const size_t at = line.find_first_of("\t ");

        if (at == std::string_view::npos || at == 0)
            continue;

        auto number = parseInt(line.substr(0, at));
        if (!number)
            continue;

        std::string_view value = line.substr(at + 1);
        while (!value.empty() && isAsciiSpace(value.front()))
            value.remove_prefix(1);

        _entries[*number] = std::string(value);
        ++added;
    }

    return added;
}

const std::string* Cliloc::get(std::int32_t number) const
{
    auto it = _entries.find(number);
    return it == _entries.end() ? nullptr : &it->second;
}

std::string Cliloc::getString(std::int32_t number) const
{
    if (const auto* s = get(number))
        return *s;
    return missingText(number);
}

std::string Cliloc::getString(std::int32_t number, std::string_view fallback, bool capitalize) const
{
    const auto* s    = get(number);
    std::string text = s ? *s : std::string(fallback);
    return capitalize ? text::capitalizeAllWords(text) : text;
}

std::string Cliloc::translate(std::int32_t number, std::string_view args, bool capitalize) const
{
    std::string text = getString(number);

    // Split the tab-separated arguments exactly as the original: leading tabs are
    // skipped, and every tab after the first non-tab character ends an argument.
    int totalArgs = 0;
    int trueStart = -1;
    const int argLen = static_cast<int>(args.size());

    for (int i = 0; i < argLen; ++i)
    {
        if (args[i] != '\t')
        {
            if (trueStart == -1)
                trueStart = i;
        }
        else if (trueStart >= 0)
        {
            ++totalArgs;
        }
    }

    if (trueStart == -1)
        trueStart = 0;

    ++totalArgs;
    std::vector<std::pair<int, int>> locations(totalArgs);

    int i = trueStart;
    for (int j = 0; i < argLen; ++i)
    {
        if (args[i] == '\t')
        {
            if (j < totalArgs - 1)
                locations[j] = {trueStart, i};
            trueStart = i + 1;
            ++j;
        }
    }

    const bool hasArguments = totalArgs - 1 > 0;
    locations[totalArgs - 1] = {trueStart, std::max(i, trueStart)};

    size_t pos = 0;

    while (pos < text.size())
    {
        const size_t open = text.find('~', pos);
        if (open == std::string::npos)
            break;

        const size_t close = text.find('~', open + 1);
        if (close == std::string::npos)
            break;

        size_t underscore = text.find('_', open + 1);
        if (underscore == std::string::npos || underscore > close)
            underscore = close;

        const size_t start = open + 1;
        size_t count       = 0;

        while (start + count < underscore && text[start + count] >= '0' && text[start + count] <= '9')
            ++count;

        auto argNumber = parseInt(std::string_view(text).substr(start, count));
        if (count == 0 || !argNumber)
            return "MegaCliloc: error for " + std::to_string(number);

        const int index = *argNumber - 1;
        std::string value;

        if (index >= 0 && index < totalArgs)
        {
            const auto [from, to] = locations[index];
            if (from <= to && to <= argLen)
                value = std::string(args.substr(from, to - from));
        }

        if (value.size() > 1)
        {
            if (value[0] == '#')
            {
                if (auto id = parseInt(std::string_view(value).substr(1)))
                    value = getString(*id);
            }
            else if (hasArguments)
            {
                if (auto id = parseInt(value))
                {
                    const auto* s = get(*id);
                    if (s && !s->empty())
                        value = *s;
                }
            }
        }

        text.replace(open, close - open + 1, value);
        pos = open;

        if (index >= 0 && index < totalArgs)
            pos += value.size();
    }

    return capitalize ? text::capitalizeAllWords(text) : text;
}

}  // namespace uo::assets
