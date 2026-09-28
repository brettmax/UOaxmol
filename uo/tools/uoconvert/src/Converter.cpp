// SPDX-License-Identifier: BSD-2-Clause
#include "Converter.h"

#include "Png.h"
#include "Stages.h"

#include "uo/assets/TileData.h"

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <thread>

namespace fs = std::filesystem;

namespace uoconvert
{

bool Context::open(std::string& error)
{
    if (!data.open(opt.uoDir))
    {
        error = "UO data folder not found: " + opt.uoDir;
        return false;
    }
    std::error_code ec;
    fs::path src = fs::weakly_canonical(opt.uoDir, ec);
    fs::path dst = fs::weakly_canonical(opt.outDir, ec);
    // The server reads the original files in place: never write into the source folder.
    auto rel = dst.lexically_relative(src);
    if (dst == src || (!rel.empty() && *rel.begin() != ".."))
    {
        error = "the output folder must be outside the UO data folder";
        return false;
    }
    if (opt.overridesDir.empty())
        opt.overridesDir = opt.uoDir;
    overrides.open(opt.overridesDir);

    if (opt.useVerdata)
    {
        std::string v = data.find("verdata.mul");
        if (!v.empty())
            verdata.load(v);
    }

    if (opt.newFormat)
        newFormat = *opt.newFormat;
    else if (std::string td = data.find("tiledata.mul"); !td.empty())
        newFormat = uo::assets::TileData::detect(fs::file_size(td, ec)) == uo::assets::TileData::Format::New;

    if (opt.jobs <= 0)
        opt.jobs = std::max(1u, std::thread::hardware_concurrency());
    fs::create_directories(opt.outDir, ec);
    return !ec;
}

std::unique_ptr<uo::io::UOFile> Context::openIndexed(const std::string& uop, const std::string& pattern,
                                                     const std::string& mul, const std::string& idx,
                                                     bool hasExtra) const
{
    std::unique_ptr<uo::io::UOFile> file;
    if (opt.useUop && !uop.empty() && data.has(uop))
        file = std::make_unique<uo::io::UopFile>(data.find(uop), pattern, hasExtra);
    else if (data.has(mul) && data.has(idx))
        file = std::make_unique<uo::io::MulFile>(data.find(mul), data.find(idx));
    if (file && !file->load())
        file.reset();
    return file;
}

EntryBytes Context::entry(const uo::io::UOFile* file, std::uint32_t index, int verdataFile) const
{
    EntryBytes out;
    if (verdataFile >= 0)
    {
        if (const auto* p = verdata.find(static_cast<std::uint32_t>(verdataFile), index))
        {
            if (p->length == 0)
                return out;  // deleted by the patch
            out.bytes  = verdata.bytes(*p);
            out.width  = static_cast<std::int16_t>(p->extra >> 16);
            out.height = static_cast<std::int16_t>(p->extra & 0xFFFF);
            return out;
        }
    }
    if (!file)
        return out;
    const uo::io::FileIndex* e = file->entry(index);
    if (!e)
        return out;
    out.width  = e->width;
    out.height = e->height;
    if (e->compression == uo::io::CompressionType::None)
    {
        out.bytes = file->raw(*e);
        return out;
    }
    bool bwt = false;
    if (!file->readDecompressed(*e, out.owned, &bwt) || bwt)
    {
        out.compressed = true;
        out.owned.clear();
        return out;
    }
    out.bytes    = out.owned;
    out.inflated = true;
    return out;
}

std::string Context::out(const std::string& relative) const
{
    fs::path p = fs::path(opt.outDir) / relative;
    fs::create_directories(p.parent_path());
    return p.string();
}

std::string Context::outDir(const std::string& relative) const
{
    fs::path p = fs::path(opt.outDir) / relative;
    fs::create_directories(p);
    return p.string();
}

void Context::warn(const std::string& message)
{
    warnings.push_back(message);
    std::fprintf(stderr, "  warning: %s\n", message.c_str());
}

std::vector<std::uint8_t> Context::readFile(const std::string& path)
{
    std::ifstream f(path, std::ios::binary);
    return std::vector<std::uint8_t>(std::istreambuf_iterator<char>(f), {});
}

const std::vector<Stage>& stages()
{
    static const std::vector<Stage> list = {
        {"hues", "hues.mul -> hue lookup texture, radarcol.mul -> colour strip", runHues},
        {"tiledata", "tiledata.mul -> data/tiledata.json", runTileData},
        {"art", "art.mul -> land and static sprite sheets", runArt},
        {"texmaps", "texmaps.mul -> ground texture sheets", runTexmaps},
        {"gumps", "gumpart.mul -> gump sprite sheets", runGumps},
        {"lights", "light.mul -> light mask sheets", runLights},
        {"maps", "map/statics -> chunked .uomap files and radar images", runMaps},
        {"multis", "multi.mul -> data/multis.json", runMultis},
        {"animdata", "animdata.mul -> data/animdata.json", runAnimData},
        {"music", "Music/ -> mp3 copies and MIDI rendered to Ogg Vorbis", runMusic},
        {"spine", "Spine exports -> validated copies under spine/", runSpine},
    };
    return list;
}

int convert(const Options& options)
{
    Context ctx(options);
    std::string error;
    if (!ctx.open(error))
    {
        std::fprintf(stderr, "uoconvert: %s\n", error.c_str());
        return 2;
    }

    JsonWriter m(true);
    m.beginObject();
    m.field("generator", "uoconvert").field("layoutVersion", 1);
    m.field("source", ctx.data.root());
    m.field("newFormat", ctx.newFormat);
    m.field("verdataPatches", static_cast<std::uint64_t>(ctx.verdata.patches().size()));
    m.key("stages").beginObject();

    int failures = 0;
    for (const Stage& s : stages())
    {
        if (!options.only.empty() && !options.only.count(s.name))
            continue;
        if (options.skip.count(s.name))
            continue;
        std::printf("%-9s %s\n", s.name, s.summary);
        std::fflush(stdout);
        auto t0 = std::chrono::steady_clock::now();
        m.key(s.name).beginObject();
        bool ran = false;
        try
        {
            ran = s.run(ctx, m);
        }
        catch (const std::exception& e)
        {
            ctx.warn(std::string(s.name) + " failed: " + e.what());
            m.field("error", e.what());
            ++failures;
        }
        double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
        m.field("ran", ran).field("seconds", secs);
        m.endObject();
    }
    m.endObject();
    m.key("warnings").beginArray();
    for (const auto& w : ctx.warnings)
        m.value(w);
    m.endArray();
    m.endObject();
    m.save(ctx.out("manifest.json"));
    std::printf("wrote %s\n", ctx.out("manifest.json").c_str());
    return failures ? 1 : 0;
}

}  // namespace uoconvert
