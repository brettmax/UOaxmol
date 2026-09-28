// SPDX-License-Identifier: BSD-2-Clause
#pragma once

#include "uo/net/Huffman.h"
#include "uo/net/PacketTable.h"

#include <cstdint>
#include <functional>
#include <span>
#include <vector>

namespace uo::net
{

// Turns a TCP byte stream from the server into whole packets. Owns Huffman decompression,
// which the game server switches on for everything after 0x91.
class PacketFramer
{
public:
    using Handler = std::function<void(std::span<const std::uint8_t> packet)>;

    explicit PacketFramer(const PacketTable& table) : _table(&table) {}

    void setCompressed(bool on)
    {
        _compressed = on;
        _huffman.reset();
    }
    bool compressed() const { return _compressed; }

    // Drops buffered bytes, e.g. when the socket reconnects to the game server.
    // Safe to call from inside a feed() handler: the rest of that feed is discarded.
    void reset()
    {
        if (_inFeed)
            _resetPending = true;
        else
            _buffer.clear();
        _huffman.reset();
    }

    // Feeds raw socket bytes; calls `handler` once per complete packet. Returns false when the
    // stream is corrupt (a variable packet claiming less than its own header), after which the
    // connection should be dropped.
    bool feed(std::span<const std::uint8_t> bytes, const Handler& handler);

private:
    const PacketTable* _table;
    HuffmanDecoder _huffman;
    bool _compressed = false;
    bool _inFeed = false;
    bool _resetPending = false;
    std::vector<std::uint8_t> _buffer;
    std::vector<std::uint8_t> _scratch;
};

}  // namespace uo::net
