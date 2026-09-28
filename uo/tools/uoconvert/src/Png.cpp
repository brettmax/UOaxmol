// SPDX-License-Identifier: BSD-2-Clause
#include "Png.h"

#include "uo/io/Compression.h"

#include <cstdlib>
#include <cstring>

namespace
{
// stb_image_write's own deflate is fast but loose; route it through uocore's zlib instead.
unsigned char* zlibCompress(unsigned char* data, int len, int* outLen, int /*quality*/)
{
    std::vector<std::uint8_t> packed = uo::io::deflate({data, static_cast<std::size_t>(len)});
    auto* out = static_cast<unsigned char*>(std::malloc(packed.size() ? packed.size() : 1));
    if (!out || packed.empty())
    {
        std::free(out);
        *outLen = 0;
        return nullptr;
    }
    std::memcpy(out, packed.data(), packed.size());
    *outLen = static_cast<int>(packed.size());
    return out;
}
}  // namespace

#define STBIW_ZLIB_COMPRESS zlibCompress
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

namespace uoconvert
{

bool writePng(const std::string& path, int width, int height, const std::uint32_t* rgba)
{
    return stbi_write_png(path.c_str(), width, height, 4, rgba, width * 4) != 0;
}

}  // namespace uoconvert
