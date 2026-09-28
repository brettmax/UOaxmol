// SPDX-License-Identifier: BSD-2-Clause
// Part of the AxmolUO client core. Ported from ClassicUO.Network (Huffman).
#pragma once

#include <cstdint>
#include <span>
#include <vector>

namespace uo::net
{

// The game server compresses everything it sends after the client logs in to it (0x91).
// The decoder is stateful: a packet can be split across TCP reads mid-symbol.
class HuffmanDecoder
{
public:
    void reset();

    // Decodes all of `src`, appending to `out`. Terminator symbols between packets are consumed
    // silently; they do not delimit anything the caller needs, since packet lengths do that.
    void decode(std::span<const std::uint8_t> src, std::vector<std::uint8_t>& out);

private:
    int _bitNum  = 8;
    int _value   = 0;
    int _mask    = 0;
    int _treePos = 0;
};

// The server side of the same code. Used by tests and by the local loopback server; it is
// derived from the decode tree, so the two can never disagree.
class HuffmanEncoder
{
public:
    // Encodes one packet followed by the terminator, padded to a byte boundary.
    static void encode(std::span<const std::uint8_t> packet, std::vector<std::uint8_t>& out);
};

}  // namespace uo::net
