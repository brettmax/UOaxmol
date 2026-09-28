// SPDX-License-Identifier: BSD-2-Clause
// The offline converter: reads a UO client folder through uocore and writes Axmol-ready assets.
#pragma once

#include "DataDir.h"
#include "Json.h"

#include "uo/io/UOFile.h"
#include "uo/io/Verdata.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <vector>

namespace uoconvert
{

struct Options
{
    std::string uoDir;            // UO client data folder (read only, never written)
    std::string outDir;           // converted asset tree
    std::string overridesDir;     // shard's loose Art/, Gumps/ folders; defaults to uoDir
    std::string spineDir;         // Spine exports to validate and copy, optional
    std::string soundfont;        // General MIDI .sf2 for rendering Music/*.mid
    std::string fluidsynth = "fluidsynth";
    std::string oggEncoder;       // "oggenc" or "ffmpeg"; auto-detected when empty
    int jobs      = 0;            // 0 = hardware threads
    int maxSize   = 2048;
    int pngLevel  = 6;
    int mapLevel  = 6;
    bool useUop     = true;
    bool useVerdata = true;
    bool radar      = true;
    std::optional<bool> newFormat;  // 7.0.9+ record layouts; detected from tiledata.mul when unset
    std::set<std::string> only, skip;
};

// One indexed asset's bytes plus the width/height the index (or a verdata patch) carried.
struct EntryBytes
{
    std::span<const std::uint8_t> bytes;
    int width  = 0;
    int height = 0;
    bool compressed = false;  // zlib/BWT UOP entry, not decodable yet
};

class Context
{
public:
    explicit Context(Options options) : opt(std::move(options)) {}

    bool open(std::string& error);

    // Opens a UOP archive when present (and allowed), else the .mul + .idx pair. nullptr if neither.
    std::unique_ptr<uo::io::UOFile> openIndexed(const std::string& uop, const std::string& pattern,
                                                const std::string& mul, const std::string& idx,
                                                bool hasExtra = false) const;

    // Entry `index` of `file`, with any verdata patch for (verdataFile, index) applied first.
    // Pass verdataFile < 0 for files ClassicUO never patches.
    EntryBytes entry(const uo::io::UOFile* file, std::uint32_t index, int verdataFile) const;

    std::string out(const std::string& relative) const;  // creates parent folders
    std::string outDir(const std::string& relative) const;
    void warn(const std::string& message);

    static std::vector<std::uint8_t> readFile(const std::string& path);

    Options opt;
    DataDir data;
    DataDir overrides;
    uo::io::Verdata verdata;
    bool newFormat = false;
    std::vector<std::string> warnings;
};

struct Stage
{
    const char* name;
    const char* summary;
    // Writes the stage's manifest members into `m` (an open object). Returns false if skipped.
    bool (*run)(Context& ctx, JsonWriter& m);
};

const std::vector<Stage>& stages();

// Runs the selected stages and writes <out>/manifest.json. Returns a process exit code.
int convert(const Options& options);

}  // namespace uoconvert
