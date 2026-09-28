// SPDX-License-Identifier: BSD-2-Clause
// Part of the AxmolUO client core. Ported from ClassicUO.Assets (ClilocLoader).
#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>

namespace uo::assets
{

// Cliloc.<lang>: { i32 header1; i16 header2; { i32 number; u8 flag; u16 length; utf8 text }* }.
class Cliloc
{
public:
    bool load(const std::string& path);
    bool loadFromBytes(std::span<const std::uint8_t> bytes);

    std::size_t size() const { return _entries.size(); }
    const std::string* get(std::int32_t number) const;

    // Substitutes ~1_NAME~, ~2_NAME~ ... with tab-separated `args`, the format the server
    // sends in 0xC1/0xCC. An argument of the form "#1234" is itself resolved as a cliloc.
    std::string format(std::int32_t number, std::string_view args) const;

private:
    std::unordered_map<std::int32_t, std::string> _entries;
};

}  // namespace uo::assets
