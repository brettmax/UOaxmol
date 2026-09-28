// SPDX-License-Identifier: BSD-2-Clause
#include "uo/net/PacketFramer.h"

namespace uo::net
{

bool PacketFramer::feed(std::span<const std::uint8_t> bytes, const Handler& handler)
{
    if (_compressed)
    {
        _scratch.clear();
        _huffman.decode(bytes, _scratch);
        _buffer.insert(_buffer.end(), _scratch.begin(), _scratch.end());
    }
    else
    {
        _buffer.insert(_buffer.end(), bytes.begin(), bytes.end());
    }

    std::size_t at = 0;
    while (at < _buffer.size())
    {
        std::uint8_t id  = _buffer[at];
        std::size_t need = 0;
        std::int16_t len = _table->length(id);

        if (len < 0)
        {
            if (_buffer.size() - at < 3)
                break;
            need = (static_cast<std::size_t>(_buffer[at + 1]) << 8) | _buffer[at + 2];
            if (need < 3)
                return false;
        }
        else if (len == 0)
        {
            return false;  // unknown id: the stream is out of sync
        }
        else
        {
            need = static_cast<std::size_t>(len);
        }

        if (_buffer.size() - at < need)
            break;

        _inFeed = true;
        handler(std::span<const std::uint8_t>(_buffer.data() + at, need));
        _inFeed = false;
        at += need;

        if (_resetPending)
        {
            _resetPending = false;
            _buffer.clear();
            return true;
        }
    }

    _buffer.erase(_buffer.begin(), _buffer.begin() + static_cast<std::ptrdiff_t>(at));
    return true;
}

}  // namespace uo::net
