// SPDX-License-Identifier: BSD-2-Clause
// Ported from ClassicUO (Assets/SpeechesLoader.cs, the keyword part of
// NetClient.Send_UnicodeSpeechRequest). speech.mul lists the words servers react to ("bank",
// "vendor buy", "guards"); clients from 3.0.5d on send the ids of the ones a line contains.

#pragma once

#include "uo/io/ClientVersion.h"

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace uo::chat
{

class SpeechKeywords
{
public:
    struct Entry
    {
        std::uint16_t id{0};
        std::vector<std::string> keywords;  // the entry split on '*'
        bool checkStart{false};             // the entry starts with '*': match anywhere
        bool checkEnd{false};               // the entry ends with '*'
    };

    // speech.mul: repeated [u16 BE id][u16 BE length][UTF-8 text]. A missing file leaves the
    // list empty, which only means speech goes out unencoded.
    bool load(const std::string& path);
    void loadFromBytes(std::span<const std::uint8_t> bytes);

    void add(std::uint16_t id, std::string_view text);
    bool empty() const noexcept { return _entries.empty(); }
    std::size_t size() const noexcept { return _entries.size(); }

    // Ids of every entry `text` matches, ascending. Clients before 3.0.5d send none.
    std::vector<std::uint16_t> match(std::string_view text, ClientVersion version) const;

    static bool isMatch(std::string_view input, const Entry& entry);

    // The keyword block of an encoded 0xAD: a 12-bit count, then 12-bit ids, nibble-packed.
    static std::vector<std::uint8_t> encode(std::span<const std::uint16_t> ids);

private:
    std::vector<Entry> _entries;
};

}  // namespace uo::chat
