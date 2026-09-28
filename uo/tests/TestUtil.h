// SPDX-License-Identifier: BSD-2-Clause
#pragma once

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#if defined(_WIN32)
#    include <process.h>
#    define getpid _getpid
#else
#    include <unistd.h>
#endif

namespace uotest
{

using Bytes = std::vector<std::uint8_t>;

inline void le16(Bytes& b, std::uint16_t v)
{
    b.push_back(v & 0xFF);
    b.push_back(v >> 8);
}
inline void le32(Bytes& b, std::uint32_t v)
{
    le16(b, v & 0xFFFF);
    le16(b, v >> 16);
}
inline void le64(Bytes& b, std::uint64_t v)
{
    le32(b, static_cast<std::uint32_t>(v));
    le32(b, static_cast<std::uint32_t>(v >> 32));
}
inline void be16(Bytes& b, std::uint16_t v)
{
    b.push_back(v >> 8);
    b.push_back(v & 0xFF);
}
inline void be32(Bytes& b, std::uint32_t v)
{
    be16(b, v >> 16);
    be16(b, v & 0xFFFF);
}
inline void ascii(Bytes& b, const std::string& s, std::size_t width)
{
    for (std::size_t i = 0; i < width; ++i)
        b.push_back(i < s.size() ? static_cast<std::uint8_t>(s[i]) : 0);
}

// A scratch directory removed when the test ends.
struct TempDir
{
    std::filesystem::path path;

    TempDir()
    {
        static int counter = 0;
        path = std::filesystem::temp_directory_path() /
               ("uocore-test-" + std::to_string(::getpid()) + "-" + std::to_string(counter++));
        std::filesystem::create_directories(path);
    }
    ~TempDir()
    {
        std::error_code ec;
        std::filesystem::remove_all(path, ec);
    }

    std::string write(const std::string& name, const Bytes& data) const
    {
        auto p = (path / name).string();
        std::ofstream(p, std::ios::binary).write(reinterpret_cast<const char*>(data.data()),
                                                 static_cast<std::streamsize>(data.size()));
        return p;
    }
};

}  // namespace uotest
