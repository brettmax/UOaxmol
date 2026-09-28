// SPDX-License-Identifier: BSD-2-Clause

#include "uo/text/Utf.h"

#include <cstdint>

namespace uo::text
{

namespace
{
constexpr char32_t kReplacement = 0xFFFD;

bool isWhiteSpace(char16_t c)
{
    switch (c)
    {
    case u' ':
    case u'\t':
    case u'\n':
    case u'\v':
    case u'\f':
    case u'\r':
    case 0x0085:
    case 0x00A0:
    case 0x1680:
    case 0x2028:
    case 0x2029:
    case 0x202F:
    case 0x205F:
    case 0x3000:
        return true;
    default:
        return c >= 0x2000 && c <= 0x200A;
    }
}

char16_t toUpper(char16_t c)
{
    if (c >= u'a' && c <= u'z')
        return static_cast<char16_t>(c - 0x20);
    // Latin-1 lower-case letters, except the division sign.
    if (c >= 0x00E0 && c <= 0x00FE && c != 0x00F7)
        return static_cast<char16_t>(c - 0x20);
    if (c == 0x00FF)
        return 0x0178;
    return c;
}
}  // namespace

std::u16string utf8ToUtf16(std::string_view text)
{
    std::u16string out;
    out.reserve(text.size());

    size_t i = 0;
    const size_t n = text.size();

    while (i < n)
    {
        const auto b0 = static_cast<uint8_t>(text[i]);
        char32_t cp   = kReplacement;
        size_t len    = 1;

        if (b0 < 0x80)
        {
            cp = b0;
        }
        else if ((b0 & 0xE0) == 0xC0)
        {
            len = 2;
        }
        else if ((b0 & 0xF0) == 0xE0)
        {
            len = 3;
        }
        else if ((b0 & 0xF8) == 0xF0)
        {
            len = 4;
        }

        if (len > 1)
        {
            if (i + len > n)
            {
                len = 1;
            }
            else
            {
                char32_t v = b0 & (0x7F >> len);
                bool ok    = true;

                for (size_t k = 1; k < len; ++k)
                {
                    const auto b = static_cast<uint8_t>(text[i + k]);

                    if ((b & 0xC0) != 0x80)
                    {
                        ok = false;
                        break;
                    }

                    v = (v << 6) | (b & 0x3F);
                }

                static constexpr char32_t kMin[] = {0, 0, 0x80, 0x800, 0x10000};

                if (!ok || v < kMin[len] || v > 0x10FFFF || (v >= 0xD800 && v <= 0xDFFF))
                {
                    len = 1;
                }
                else
                {
                    cp = v;
                }
            }
        }

        if (cp >= 0x10000)
        {
            cp -= 0x10000;
            out.push_back(static_cast<char16_t>(0xD800 + (cp >> 10)));
            out.push_back(static_cast<char16_t>(0xDC00 + (cp & 0x3FF)));
        }
        else
        {
            out.push_back(static_cast<char16_t>(cp));
        }

        i += len;
    }

    return out;
}

std::string utf16ToUtf8(std::u16string_view text)
{
    std::string out;
    out.reserve(text.size());

    for (size_t i = 0; i < text.size(); ++i)
    {
        char32_t cp = text[i];

        if (cp >= 0xD800 && cp <= 0xDBFF && i + 1 < text.size() && text[i + 1] >= 0xDC00 && text[i + 1] <= 0xDFFF)
        {
            cp = 0x10000 + ((cp - 0xD800) << 10) + (text[i + 1] - 0xDC00);
            ++i;
        }
        else if (cp >= 0xD800 && cp <= 0xDFFF)
        {
            cp = kReplacement;
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

    return out;
}

std::u16string capitalizeAllWords(std::u16string_view text)
{
    std::u16string out;
    out.reserve(text.size());
    bool capitalizeNext = true;

    for (size_t i = 0; i < text.size(); ++i)
    {
        out.push_back(capitalizeNext ? toUpper(text[i]) : text[i]);

        if (!isWhiteSpace(text[i]))
        {
            capitalizeNext = i + 1 < text.size() && isWhiteSpace(text[i + 1]);
        }
    }

    return out;
}

std::string capitalizeAllWords(std::string_view utf8)
{
    return utf16ToUtf8(capitalizeAllWords(utf8ToUtf16(utf8)));
}

}  // namespace uo::text
