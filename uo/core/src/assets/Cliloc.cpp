// SPDX-License-Identifier: BSD-2-Clause
#include "uo/assets/Cliloc.h"

#include "uo/io/BinaryReader.h"
#include "uo/io/MappedFile.h"

#include <algorithm>
#include <cstdlib>
#include <vector>

namespace uo::assets
{

bool Cliloc::load(const std::string& path)
{
    io::MappedFile f(path);
    if (!f.isOpen())
        return false;
    return loadFromBytes(f.bytes());
}

bool Cliloc::loadFromBytes(std::span<const std::uint8_t> bytes)
{
    // 7.0.100+ clilocs are BWT-compressed (byte 3 == 0x8E). Those installs are not the
    // Second Age target; the conversion pipeline can pre-decompress them.
    if (bytes.size() > 3 && bytes[3] == 0x8E)
        return false;

    io::BinaryReader r(bytes);
    r.readI32LE();
    r.readI16LE();
    _entries.clear();

    while (r.remaining() >= 7)
    {
        std::int32_t number = r.readI32LE();
        r.readU8();  // flag
        std::uint16_t length = r.readU16LE();
        auto text            = r.rest().first(std::min<std::size_t>(length, r.remaining()));
        _entries[number].assign(reinterpret_cast<const char*>(text.data()), text.size());
        r.skip(length);
    }
    return !_entries.empty();
}

const std::string* Cliloc::get(std::int32_t number) const
{
    auto it = _entries.find(number);
    return it == _entries.end() ? nullptr : &it->second;
}

std::string Cliloc::format(std::int32_t number, std::string_view args) const
{
    const std::string* base = get(number);
    if (!base)
        return {};

    std::vector<std::string> parts;
    std::size_t start = 0;
    while (start <= args.size() && !args.empty())
    {
        std::size_t tab = args.find('\t', start);
        std::string arg(args.substr(start, tab == std::string_view::npos ? std::string_view::npos : tab - start));
        if (arg.size() > 1 && arg[0] == '#')
        {
            if (const std::string* nested = get(std::atoi(arg.c_str() + 1)))
                arg = *nested;
        }
        parts.push_back(std::move(arg));
        if (tab == std::string_view::npos)
            break;
        start = tab + 1;
    }

    std::string out;
    out.reserve(base->size());
    for (std::size_t i = 0; i < base->size(); ++i)
    {
        if ((*base)[i] == '~')
        {
            std::size_t close = base->find('~', i + 1);
            if (close != std::string::npos)
            {
                int n = std::atoi(base->c_str() + i + 1);
                if (n > 0)
                {
                    if (static_cast<std::size_t>(n) <= parts.size())
                        out += parts[n - 1];
                    i = close;
                    continue;
                }
            }
        }
        out.push_back((*base)[i]);
    }
    return out;
}

}  // namespace uo::assets
