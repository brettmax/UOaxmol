// SPDX-License-Identifier: BSD-2-Clause
// Part of the AxmolUO client core. Ported from ClassicUO.Assets (VerdataLoader).
#pragma once

#include "uo/io/MappedFile.h"

#include <cstdint>
#include <string>
#include <vector>

namespace uo::assets
{

// verdata.mul: patches for other MUL files, shipped by pre-5.0 clients and by free shards.
struct VerdataPatch
{
    std::uint32_t fileId   = 0;
    std::uint32_t blockId  = 0;
    std::uint32_t position = 0; // offset inside verdata.mul
    std::uint32_t length   = 0;
    std::uint32_t extra    = 0;
};

class Verdata
{
public:
    // FileIDs: 0 map0, 1 staidx0, 2 statics0, 3 artidx, 4 art, 5 anim.idx, 6 anim.mul, 7 soundidx,
    // 8 sound, 9 texidx, 10 texmaps, 11 gumpidx, 12 gumps, 13 multi.idx, 14 multi, 15 skills.idx,
    // 16 skills, 30 tiledata, 31 animdata.
    static constexpr std::uint32_t FILE_ID_ANIM_MUL = 6;

    bool load(const std::string& path);
    bool isOpen() const { return _file.isOpen(); }

    const std::vector<VerdataPatch>& patches() const { return _patches; }
    const io::MappedFile& file() const { return _file; }

private:
    io::MappedFile _file;
    std::vector<VerdataPatch> _patches;
};

}  // namespace uo::assets
