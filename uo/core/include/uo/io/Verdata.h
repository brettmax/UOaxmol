// SPDX-License-Identifier: BSD-2-Clause
// Part of the AxmolUO client core. Ported from ClassicUO.Assets (VerdataLoader) and the patch
// loop in UOFileManager.
#pragma once

#include "uo/io/MappedFile.h"

#include <cstdint>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

namespace uo::io
{

// verdata.mul: { i32 count; count x { u32 fileId, blockId, position, length, extra } } and
// the patch bytes. T2A-era clients ship it and apply it over the other files; a patch of
// length 0 deletes that block. Lookup is per (file, block); the last patch for a block wins.
class Verdata
{
public:
    enum FileId : std::uint32_t
    {
        Map0     = 0,
        StaIdx0  = 1,
        Statics0 = 2,
        ArtIdx   = 3,
        Art      = 4,
        AnimIdx  = 5,
        Anim     = 6,
        SoundIdx = 7,
        Sound    = 8,
        TexIdx   = 9,
        Texmaps  = 10,
        GumpIdx  = 11,
        Gumps    = 12,
        MultiIdx = 13,
        Multi    = 14,
        SkillIdx = 15,
        Skills   = 16,
        TileData = 30,
        AnimData = 31,
        Hues     = 32,
    };

    struct Patch
    {
        std::uint32_t fileId   = 0;
        std::uint32_t blockId  = 0;
        std::uint32_t position = 0;
        std::uint32_t length   = 0;
        std::uint32_t extra    = 0;  // gumps: width << 16 | height
    };

    bool load(const std::string& path);
    bool loaded() const { return _file.isOpen() && !_patches.empty(); }

    const std::vector<Patch>& patches() const { return _patches; }
    const Patch* find(std::uint32_t fileId, std::uint32_t blockId) const;
    std::span<const std::uint8_t> bytes(const Patch& p) const { return _file.slice(p.position, p.length); }

private:
    MappedFile _file;
    std::vector<Patch> _patches;
    std::unordered_map<std::uint64_t, std::size_t> _byKey;
};

}  // namespace uo::io
