// SPDX-License-Identifier: BSD-2-Clause
// Ported from ClassicUO (Assets/SpeechesLoader.cs, Network/OutgoingPackets.cs
// Send_UnicodeSpeechRequest).

#include "uo/chat/SpeechKeywords.h"

#include <algorithm>
#include <fstream>
#include <iterator>

namespace uo::chat
{

namespace
{

char lower(char c)
{
    return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c;
}

// Case-insensitive find of `needle` in `hay` starting at `from` (ASCII case folding, which is
// what the keyword list uses).
std::size_t findNoCase(std::string_view hay, std::string_view needle, std::size_t from)
{
    if (needle.size() > hay.size())
    {
        return std::string_view::npos;
    }
    for (std::size_t i = from; i + needle.size() <= hay.size(); ++i)
    {
        std::size_t k = 0;
        while (k < needle.size() && lower(hay[i + k]) == lower(needle[k]))
        {
            ++k;
        }
        if (k == needle.size())
        {
            return i;
        }
    }
    return std::string_view::npos;
}

bool equalsNoCase(std::string_view a, std::string_view b)
{
    return a.size() == b.size() && findNoCase(a, b, 0) == 0;
}

// char.IsWhiteSpace || !char.IsLetter: a word boundary. Bytes of multi-byte UTF-8 sequences
// count as letters, as accented letters do in the original.
bool isBoundary(char c)
{
    const auto u = static_cast<unsigned char>(c);
    if (u >= 0x80)
    {
        return false;
    }
    return !((u >= 'a' && u <= 'z') || (u >= 'A' && u <= 'Z'));
}

}  // namespace

bool SpeechKeywords::load(const std::string& path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in)
    {
        return false;
    }
    const std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
    loadFromBytes(bytes);
    return true;
}

void SpeechKeywords::loadFromBytes(std::span<const std::uint8_t> bytes)
{
    _entries.clear();
    std::size_t pos = 0;
    while (pos + 4 <= bytes.size())
    {
        const auto id     = static_cast<std::uint16_t>((bytes[pos] << 8) | bytes[pos + 1]);
        const auto length = static_cast<std::size_t>((bytes[pos + 2] << 8) | bytes[pos + 3]);
        pos += 4;
        if (pos + length > bytes.size())
        {
            break;
        }
        if (length > 0)
        {
            add(id, std::string_view(reinterpret_cast<const char*>(bytes.data() + pos), length));
        }
        pos += length;
    }
}

void SpeechKeywords::add(std::uint16_t id, std::string_view text)
{
    Entry e;
    e.id         = id;
    e.checkStart = !text.empty() && text.front() == '*';
    e.checkEnd   = !text.empty() && text.back() == '*';

    std::size_t start = 0;
    while (start <= text.size())
    {
        const std::size_t star = text.find('*', start);
        const std::size_t end  = star == std::string_view::npos ? text.size() : star;
        if (end > start)
        {
            e.keywords.emplace_back(text.substr(start, end - start));
        }
        if (star == std::string_view::npos)
        {
            break;
        }
        start = star + 1;
    }
    _entries.push_back(std::move(e));
}

bool SpeechKeywords::isMatch(std::string_view input, const Entry& entry)
{
    for (const std::string& word : entry.keywords)
    {
        if (word.size() > input.size() || word.empty())
        {
            continue;
        }

        // Without a leading '*' the line must start with the word, without a trailing one it
        // must end with it.
        if (!entry.checkStart && !equalsNoCase(input.substr(0, word.size()), word))
        {
            continue;
        }
        if (!entry.checkEnd && !equalsNoCase(input.substr(input.size() - word.size()), word))
        {
            continue;
        }

        // "bank", " bank", "bank ", "!bank", "bank!": the word must stand on its own.
        std::size_t idx = findNoCase(input, word, 0);
        while (idx != std::string_view::npos)
        {
            const bool before = idx == 0 || isBoundary(input[idx - 1]);
            const bool after  = idx + word.size() >= input.size() || isBoundary(input[idx + word.size()]);
            if (before && after)
            {
                return true;
            }
            idx = findNoCase(input, word, idx + 1);
        }
    }
    return false;
}

std::vector<std::uint16_t> SpeechKeywords::match(std::string_view text, ClientVersion version) const
{
    std::vector<std::uint16_t> ids;
    if (version < makeVersion(3, 0, 5, 'd'))
    {
        return ids;
    }

    while (!text.empty() && text.front() == ' ')
    {
        text.remove_prefix(1);
    }
    while (!text.empty() && text.back() == ' ')
    {
        text.remove_suffix(1);
    }

    for (const Entry& e : _entries)
    {
        if (isMatch(text, e))
        {
            ids.push_back(e.id);
        }
    }
    std::stable_sort(ids.begin(), ids.end());
    return ids;
}

std::vector<std::uint8_t> SpeechKeywords::encode(std::span<const std::uint16_t> ids)
{
    std::vector<std::uint8_t> out;
    const auto count = static_cast<int>(ids.size());
    out.push_back(static_cast<std::uint8_t>(count >> 4));
    int nibble = count & 15;
    bool flag  = false;

    for (const std::uint16_t id : ids)
    {
        if (flag)
        {
            out.push_back(static_cast<std::uint8_t>(id >> 4));
            nibble = id & 15;
        }
        else
        {
            out.push_back(static_cast<std::uint8_t>((nibble << 4) | ((id >> 8) & 15)));
            out.push_back(static_cast<std::uint8_t>(id));
        }
        flag = !flag;
    }

    if (!flag)
    {
        out.push_back(static_cast<std::uint8_t>(nibble << 4));
    }
    return out;
}

}  // namespace uo::chat
