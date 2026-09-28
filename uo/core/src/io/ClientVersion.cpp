// SPDX-License-Identifier: BSD-2-Clause
#include "uo/io/ClientVersion.h"

#include <cctype>

namespace uo
{

std::optional<ClientVersion> parseClientVersion(std::string_view text)
{
    std::uint32_t parts[4] = {0, 0, 0, 0};
    int part               = 0;
    bool haveDigit         = false;

    for (std::size_t i = 0; i < text.size(); ++i)
    {
        char c = text[i];
        if (std::isdigit(static_cast<unsigned char>(c)))
        {
            parts[part] = parts[part] * 10 + static_cast<std::uint32_t>(c - '0');
            if (parts[part] > 255)
                return std::nullopt;
            haveDigit = true;
        }
        else if (c == '.')
        {
            if (!haveDigit || part == 3)
                return std::nullopt;
            ++part;
            haveDigit = false;
        }
        else if (std::isalpha(static_cast<unsigned char>(c)) && part == 2 && haveDigit && i + 1 == text.size())
        {
            // "4.0.11d": the letter is the revision
            parts[3] = static_cast<std::uint32_t>(std::tolower(static_cast<unsigned char>(c)));
        }
        else
        {
            return std::nullopt;
        }
    }

    if (!haveDigit || part < 2)
        return std::nullopt;

    return makeVersion(parts[0], parts[1], parts[2], parts[3]);
}

std::string clientVersionToString(ClientVersion v)
{
    std::uint32_t d = v & 0xFF;
    std::string s   = std::to_string(v >> 24) + "." + std::to_string((v >> 16) & 0xFF) + "." +
                    std::to_string((v >> 8) & 0xFF);
    if (d >= 'a' && d <= 'z')
        s.push_back(static_cast<char>(d));
    else
        s += "." + std::to_string(d);
    return s;
}

}  // namespace uo
