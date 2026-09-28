// SPDX-License-Identifier: BSD-2-Clause
// Ported from ClassicUO (ClassicUO.IO/DefReader.cs).
#include "uo/io/DefReader.h"

#include <charconv>
#include <fstream>
#include <sstream>

namespace uo::io
{

namespace
{

bool isDigit(char c)
{
    return c >= '0' && c <= '9';
}

std::string_view trim(std::string_view s)
{
    const auto ws = " \t\r\n\v\f";
    const auto b  = s.find_first_not_of(ws);
    if (b == std::string_view::npos)
    {
        return {};
    }
    const auto e = s.find_last_not_of(ws);
    return s.substr(b, e - b + 1);
}

void split(std::string_view s, std::string_view seps, std::vector<std::string>& out)
{
    size_t i = 0;
    while (i < s.size())
    {
        while (i < s.size() && seps.find(s[i]) != std::string_view::npos)
        {
            ++i;
        }
        size_t j = i;
        while (j < s.size() && seps.find(s[j]) == std::string_view::npos)
        {
            ++j;
        }
        if (j > i)
        {
            out.emplace_back(s.substr(i, j - i));
        }
        i = j;
    }
}

std::string_view sanitizeNumber(std::string_view token)
{
    for (size_t i = 0; i < token.size(); ++i)
    {
        const char c = token[i];
        if (!isDigit(c) && c != '-' && c != '+')
        {
            return token.substr(0, i);
        }
    }
    return token;
}

std::optional<int> parseInt(std::string_view s)
{
    if (!s.empty() && s[0] == '+')
    {
        s.remove_prefix(1);
    }
    int v          = 0;
    auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), v);
    if (ec != std::errc{} || ptr != s.data() + s.size() || s.empty())
    {
        return std::nullopt;
    }
    return v;
}

bool isGroup(std::string_view s)
{
    return !s.empty() && s.front() == '{' && s.back() == '}';
}

constexpr std::string_view kTokens      = "\t ";
constexpr std::string_view kGroupTokens = ", {}";

} // namespace

DefReader DefReader::fromFile(const std::filesystem::path& path, int minSize)
{
    std::ifstream in(path, std::ios::binary);
    std::stringstream ss;
    ss << in.rdbuf();
    return fromString(ss.str(), minSize);
}

DefReader DefReader::fromString(std::string_view text, int minSize)
{
    DefReader r;

    size_t start = 0;
    while (start <= text.size())
    {
        size_t end = text.find('\n', start);
        if (end == std::string_view::npos)
        {
            end = text.size();
        }

        std::string_view line = trim(text.substr(start, end - start));
        start                 = end + 1;

        if (line.empty() || line[0] == '#' || !isDigit(line[0]))
        {
            if (end == text.size())
            {
                break;
            }
            continue;
        }

        if (const auto comment = line.find('#'); comment != std::string_view::npos)
        {
            line = line.substr(0, comment);
        }

        std::vector<std::string> parts;
        const auto groupStart = line.find('{');
        const auto groupEnd   = line.find('}');

        if (groupStart != std::string_view::npos && groupEnd != std::string_view::npos && groupEnd >= groupStart)
        {
            split(line.substr(0, groupStart), kTokens, parts);
            parts.emplace_back(line.substr(groupStart, groupEnd - groupStart + 1));
            split(line.substr(groupEnd + 1), kTokens, parts);
        }
        else
        {
            split(line, kTokens, parts);
        }

        if (static_cast<int>(parts.size()) >= minSize)
        {
            r._lines.push_back(std::move(parts));
        }

        if (end == text.size())
        {
            break;
        }
    }

    return r;
}

bool DefReader::next()
{
    if (_line + 1 < linesCount())
    {
        ++_line;
        _position = 0;
        return true;
    }
    return false;
}

int DefReader::partsCount() const
{
    if (_line < 0 || _line >= linesCount())
    {
        return 0;
    }
    return static_cast<int>(_lines[_line].size());
}

const std::string& DefReader::tokenAt(int line, int index) const
{
    static const std::string zero = "0";
    if (line < 0 || line >= linesCount())
    {
        return zero;
    }
    const auto& p = _lines[line];
    if (index < 0 || index >= static_cast<int>(p.size()))
    {
        return zero;
    }
    return p[index];
}

int DefReader::readInt()
{
    const std::string_view token = sanitizeNumber(tokenAt(_line, _position++));
    if (token.empty())
    {
        return -1;
    }
    return parseInt(token).value_or(-1);
}

std::optional<int> DefReader::readGroupInt(int index)
{
    const std::string& token = tokenAt(_line, _position++);
    if (!isGroup(token))
    {
        return std::nullopt;
    }

    std::vector<std::string> group;
    split(token, kGroupTokens, group);

    if (index < 0 || index >= static_cast<int>(group.size()))
    {
        return std::nullopt;
    }

    return parseInt(sanitizeNumber(group[index]));
}

std::optional<std::vector<int>> DefReader::readGroup()
{
    const std::string& token = tokenAt(_line, _position++);
    if (!isGroup(token))
    {
        return std::nullopt;
    }

    std::vector<std::string> parts;
    split(token, kGroupTokens, parts);

    std::vector<int> result;
    for (const auto& p : parts)
    {
        if (p.empty() || !isDigit(p[0]))
        {
            continue;
        }
        // ClassicUO parses "0x.." tokens with NumberStyles.HexNumber, which rejects the
        // prefix, so they are dropped; plain decimals are kept.
        if (auto v = parseInt(p))
        {
            result.push_back(*v);
        }
    }
    return result;
}

} // namespace uo::io
