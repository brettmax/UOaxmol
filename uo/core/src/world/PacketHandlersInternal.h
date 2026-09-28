// SPDX-License-Identifier: BSD-2-Clause
// String helpers matching ClassicUO's StackDataReader semantics on top of io::BinaryReader.
#pragma once

#include "uo/io/BinaryReader.h"
#include "uo/world/World.h"

#include <optional>
#include <string>
#include <string_view>

namespace uo::world::detail
{

// ClassicUO ReadASCII(len): len == 0 reads nothing, len < 0 reads to NUL/end,
// len > 0 reads a fixed-width field. (BinaryReader treats width 0 as "to NUL".) Packet text
// is Windows-1252, returned as UTF-8.
inline std::string ascii(io::BinaryReader& r, int len)
{
    if (len == 0)
        return {};
    return r.readASCII(len < 0 ? 0 : size_t(len), true);
}

// UTF-8 fields are byte strings on the wire, copied as they are.
inline std::string utf8(io::BinaryReader& r, int len, bool safe = true)
{
    std::string s = len == 0 ? std::string() : r.readASCII(len < 0 ? 0 : size_t(len));
    if (safe)
    {
        std::erase_if(s, [](char c) {
            auto u = static_cast<unsigned char>(c);
            return u < 0x20 && u != '\n' && u != '\r' && u != '\t';
        });
    }
    return s;
}

inline std::string unicodeBE(io::BinaryReader& r, int chars)
{
    if (chars == 0)
        return {};
    return r.readUnicodeBE(chars < 0 ? 0 : size_t(chars));
}

inline std::string unicodeLE(io::BinaryReader& r, int chars)
{
    if (chars == 0)
        return {};
    return r.readUnicodeLE(chars < 0 ? 0 : size_t(chars));
}

// Cliloc text, or `fallback` when no resolver is attached or the number is unknown.
inline std::string clilocOr(World& world, uint32_t cliloc, std::string_view fallback)
{
    if (ClilocResolver* c = world.clilocs())
    {
        std::string s = c->get(cliloc);
        if (!s.empty())
            return s;
    }
    return std::string(fallback);
}

// Translated cliloc with arguments. Without a resolver the text is "#<number>" followed by the
// raw arguments, so logs and tests still show what arrived.
inline std::optional<std::string> translate(World& world, uint32_t cliloc, std::string_view args, bool capitalize)
{
    if (ClilocResolver* c = world.clilocs())
        return c->translate(cliloc, args, capitalize);

    std::string s = "#" + std::to_string(cliloc);
    if (!args.empty())
    {
        s += ' ';
        s += args;
    }
    return s;
}

} // namespace uo::world::detail
