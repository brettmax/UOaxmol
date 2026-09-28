// SPDX-License-Identifier: BSD-2-Clause
// uoconvert: converts a UO client folder into assets Axmol loads natively. See uo/tools/README.md.
#include "Converter.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <set>
#include <string>
#include <sstream>

namespace
{

void usage()
{
    std::printf(
        "usage: uoconvert --uo <client folder> --out <asset folder> [options]\n"
        "\n"
        "  --overrides <dir>    shard's loose Art/, Gumps/ folders (default: the client folder)\n"
        "  --only a,b,...       run only these stages\n"
        "  --skip a,b,...       skip these stages\n"
        "  --jobs <n>           pages rendered in parallel (default: all cores)\n"
        "  --max-size <px>      sprite-sheet page edge (default 2048)\n"
        "  --png-level <0-9>    PNG zlib level (default 6)\n"
        "  --map-level <0-9>    .uomap chunk zlib level (default 6)\n"
        "  --no-uop             ignore *LegacyMUL.uop archives\n"
        "  --no-verdata         ignore verdata.mul\n"
        "  --no-radar           skip the per-map radar PNGs\n"
        "  --new-format | --old-format   force 7.0.9+ / older record layouts\n"
        "  --soundfont <sf2>    General MIDI soundfont for rendering Music/*.mid to Ogg\n"
        "  --fluidsynth <exe>   FluidSynth binary (default: fluidsynth)\n"
        "  --ogg-encoder <exe>  oggenc or ffmpeg (default: whichever is on PATH)\n"
        "  --spine <dir>        Spine exports to validate and copy into spine/\n"
        "  --list               list the stages and exit\n");
}

std::set<std::string> csv(const char* s)
{
    std::set<std::string> out;
    std::stringstream ss(s);
    for (std::string t; std::getline(ss, t, ',');)
        if (!t.empty())
            out.insert(t);
    return out;
}

}  // namespace

int main(int argc, char** argv)
{
    uoconvert::Options o;
    for (int i = 1; i < argc; ++i)
    {
        std::string a = argv[i];
        auto next = [&]() -> const char* {
            if (i + 1 >= argc)
            {
                std::fprintf(stderr, "uoconvert: %s needs a value\n", a.c_str());
                std::exit(2);
            }
            return argv[++i];
        };
        if (a == "--uo") o.uoDir = next();
        else if (a == "--out") o.outDir = next();
        else if (a == "--overrides") o.overridesDir = next();
        else if (a == "--only") o.only = csv(next());
        else if (a == "--skip") o.skip = csv(next());
        else if (a == "--jobs") o.jobs = std::atoi(next());
        else if (a == "--max-size") o.maxSize = std::atoi(next());
        else if (a == "--png-level") o.pngLevel = std::atoi(next());
        else if (a == "--map-level") o.mapLevel = std::atoi(next());
        else if (a == "--no-uop") o.useUop = false;
        else if (a == "--no-verdata") o.useVerdata = false;
        else if (a == "--no-radar") o.radar = false;
        else if (a == "--new-format") o.newFormat = true;
        else if (a == "--old-format") o.newFormat = false;
        else if (a == "--soundfont") o.soundfont = next();
        else if (a == "--fluidsynth") o.fluidsynth = next();
        else if (a == "--ogg-encoder") o.oggEncoder = next();
        else if (a == "--spine") o.spineDir = next();
        else if (a == "--list")
        {
            for (const auto& s : uoconvert::stages())
                std::printf("%-9s %s\n", s.name, s.summary);
            return 0;
        }
        else if (a == "-h" || a == "--help")
        {
            usage();
            return 0;
        }
        else
        {
            std::fprintf(stderr, "uoconvert: unknown option %s\n", a.c_str());
            usage();
            return 2;
        }
    }
    if (o.uoDir.empty() || o.outDir.empty())
    {
        usage();
        return 2;
    }
    if (o.maxSize < 256 || o.maxSize > 16384)
    {
        std::fprintf(stderr, "uoconvert: --max-size must be 256..16384\n");
        return 2;
    }
    for (const auto& name : o.only)
    {
        bool known = false;
        for (const auto& s : uoconvert::stages())
            known |= name == s.name;
        if (!known)
        {
            std::fprintf(stderr, "uoconvert: unknown stage %s (see --list)\n", name.c_str());
            return 2;
        }
    }
    return uoconvert::convert(o);
}
