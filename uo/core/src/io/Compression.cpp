// SPDX-License-Identifier: BSD-2-Clause
#include "uo/io/Compression.h"

#include <zlib.h>

namespace uo::io
{

bool inflate(std::span<const std::uint8_t> src, std::vector<std::uint8_t>& out, std::size_t expectedSize)
{
    out.clear();
    z_stream zs{};
    if (inflateInit(&zs) != Z_OK)
        return false;

    zs.next_in  = const_cast<Bytef*>(src.data());
    zs.avail_in = static_cast<uInt>(src.size());

    std::size_t chunk = expectedSize ? expectedSize : src.size() * 4 + 64;
    int rc            = Z_OK;
    while (rc == Z_OK)
    {
        std::size_t have = out.size();
        out.resize(have + chunk);
        zs.next_out  = out.data() + have;
        zs.avail_out = static_cast<uInt>(chunk);
        rc           = ::inflate(&zs, Z_NO_FLUSH);
        out.resize(have + chunk - zs.avail_out);
        if (rc == Z_BUF_ERROR && zs.avail_in == 0)
            break;  // truncated input
    }
    inflateEnd(&zs);
    return rc == Z_STREAM_END;
}

std::vector<std::uint8_t> deflate(std::span<const std::uint8_t> src)
{
    uLongf size = compressBound(static_cast<uLong>(src.size()));
    std::vector<std::uint8_t> out(size);
    if (compress(out.data(), &size, src.data(), static_cast<uLong>(src.size())) != Z_OK)
        return {};
    out.resize(size);
    return out;
}

}  // namespace uo::io
