// SPDX-License-Identifier: BSD-2-Clause
#include "Png.h"

#include <cstdlib>
#include <cstring>
#include <zlib.h>

namespace
{
int g_level = 6;

// stb_image_write's own deflate is fast but loose; route it through zlib instead.
unsigned char* zlibCompress(unsigned char* data, int len, int* outLen, int /*quality*/)
{
    uLongf size = compressBound(static_cast<uLong>(len));
    auto* out   = static_cast<unsigned char*>(std::malloc(size));
    if (!out || compress2(out, &size, data, static_cast<uLong>(len), g_level) != Z_OK)
    {
        std::free(out);
        *outLen = 0;
        return nullptr;
    }
    *outLen = static_cast<int>(size);
    return out;
}
}  // namespace

#define STBIW_ZLIB_COMPRESS zlibCompress
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

namespace uoconvert
{

void setPngCompression(int level)
{
    g_level = level < 0 ? 0 : (level > 9 ? 9 : level);
}

bool writePng(const std::string& path, int width, int height, const std::uint32_t* rgba)
{
    return stbi_write_png(path.c_str(), width, height, 4, rgba, width * 4) != 0;
}

std::vector<std::uint8_t> deflate(const std::uint8_t* data, std::size_t size, int level)
{
    uLongf bound = compressBound(static_cast<uLong>(size));
    std::vector<std::uint8_t> out(bound);
    if (compress2(out.data(), &bound, data, static_cast<uLong>(size), level) != Z_OK)
        return {};
    out.resize(bound);
    return out;
}

}  // namespace uoconvert
