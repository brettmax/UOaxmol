// SPDX-License-Identifier: BSD-2-Clause

#pragma once

#include <string>
#include <string_view>

namespace uo::text
{

// UTF-8 <-> UTF-16 conversion. Invalid sequences decode to U+FFFD. Code points
// above the BMP become surrogate pairs; the UO fonts only cover the BMP, so those
// render as missing glyphs, exactly as they did in the original client.
std::u16string utf8ToUtf16(std::string_view text);
std::string utf16ToUtf8(std::u16string_view text);

// Upper-cases the first letter of every whitespace-separated word
// (StringHelper.CapitalizeAllWords). Only ASCII and Latin-1 letters change case.
std::u16string capitalizeAllWords(std::u16string_view text);
std::string capitalizeAllWords(std::string_view utf8);

}  // namespace uo::text
