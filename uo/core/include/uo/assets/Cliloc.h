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

class Installation;

// Localized string table (Cliloc.enu and friends), stored as UTF-8.
//
// Cliloc.<lang>: { i32 header1; i16 header2; { i32 number; u8 flag; u16 length; utf8 text }* },
// BWT-compressed on 7.x clients (byte 3 == 0x8E). Later loads replace entries with the same
// number, which is how the original layers the language file over Cliloc.enu and a shard's
// plain-text Clilocs.txt over both.
class Cliloc
{
public:
    // One cliloc file, added on top of what is already loaded.
    bool load(const std::string& path);
    // Cliloc.<lang> from the installation (Cliloc.enu when that language is missing), with
    // Cliloc.enu underneath when lang is not "enu", then Clilocs.txt on top. Installations
    // older than Cliloc.enu fall back to cliloc-1.<lang> (or cliloc-1.enu).
    bool load(const Installation& installation, std::string_view lang);
    // One cliloc file image, plain or BWT-compressed, added on top of what is loaded.
    bool loadFromBytes(std::span<const std::uint8_t> bytes);
    // A pre-Cliloc.enu string table (cliloc-1.enu in 1.x clients): an IFF image
    // FORM DATA { FORM LANG { INFO, TEXT } } whose TEXT is NUL-separated strings, Latin-1 or
    // UTF-16LE as INFO says.
    // String i becomes entry `base + i`; cliloc-1 holds 500000 and up, the numbers
    // ModernUO still sends for those messages.
    bool loadLegacyFromBytes(std::span<const std::uint8_t> bytes, std::int32_t base);
    // Clilocs.txt content: "<number><tab or spaces><text>" per line, '#' comments.
    // Returns the number of entries added.
    int loadOverrides(std::string_view text);

    void clear() { _entries.clear(); }
    std::size_t size() const { return _entries.size(); }
    // Every loaded entry (number to UTF-8 text), for offline export.
    const std::unordered_map<std::int32_t, std::string>& entries() const { return _entries; }
    const std::string* get(std::int32_t number) const;

    // Entry, else "MegaCliloc: missing <n> [~1_val~] [~2_val~]".
    std::string getString(std::int32_t number) const;
    // Entry, else `fallback`; optionally with every word capitalized.
    std::string getString(std::int32_t number, std::string_view fallback, bool capitalize = false) const;

    // Substitutes ~1_NAME~, ~2_NAME~ ... with the tab-separated `args` the server sends in
    // 0xC1/0xCC/0xD6, with the original client's rules: leading tabs are skipped, "#1234"
    // is replaced by that cliloc, and when there are several arguments a plain number that
    // names an existing cliloc is replaced by it too.
    std::string translate(std::int32_t number, std::string_view args = {}, bool capitalize = false) const;

    // translate() without capitalization.
    std::string format(std::int32_t number, std::string_view args) const { return translate(number, args); }

private:
    std::unordered_map<std::int32_t, std::string> _entries;
};

}  // namespace uo::assets
