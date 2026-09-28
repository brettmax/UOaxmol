// SPDX-License-Identifier: BSD-2-Clause
// Mobile, mount and equipment animations, one sheet set per body, decoded by
// uo::anim::AnimationsLoader (anim*.mul with verdata, or AnimationFrame*.uop).
#include "Atlas.h"
#include "Stages.h"

#include "uo/anim/AnimationsLoader.h"
#include "uo/assets/Verdata.h"

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <filesystem>
#include <map>
#include <mutex>
#include <thread>

namespace fs = std::filesystem;

namespace uoconvert
{

namespace
{

using namespace uo::anim;

std::string bodyName(std::uint32_t body)
{
    char buf[16];
    std::snprintf(buf, sizeof(buf), "0x%04X", body);
    return buf;
}

// Sprite ids inside one body's sheets: action, direction and frame number.
std::uint32_t frameId(int action, int dir, int frame)
{
    return (static_cast<std::uint32_t>(action) << 16) | (static_cast<std::uint32_t>(dir) << 8) |
           static_cast<std::uint32_t>(frame);
}

struct BodyResult
{
    std::uint32_t body = 0;
    AnimationsLoader::Indices indices;
    int actions = 0;  // actions holding at least one frame
    AtlasResult atlas;
};

// Decodes every action and direction of `body` and writes anims/<body>.json and its sheets.
BodyResult convertBody(const AnimationsLoader& loader, std::uint32_t body, AnimationsLoader::Indices indices,
                       const std::string& dir, const AtlasOptions& options)
{
    BodyResult r;
    r.body = body;
    const bool uop = (indices.flags & AnimationFlags::UseUopAnimation) != 0;
    const int actionCount =
        uop ? static_cast<int>(indices.directions.size()) : static_cast<int>(indices.directions.size()) / MAX_DIRECTIONS;

    std::map<std::uint32_t, FrameInfo> frames;
    for (int a = 0; a < actionCount; ++a)
    {
        std::array<std::vector<FrameInfo>, MAX_DIRECTIONS> dirs;
        if (uop)
        {
            const auto& index = indices.directions[static_cast<std::size_t>(a)];
            if (index.size == 0)
                continue;
            dirs = loader.readUopAnimationFramesAllDirections(static_cast<std::uint16_t>(body), static_cast<std::uint8_t>(a),
                                                              indices.type, indices.fileIndex, index);
        }
        else
        {
            for (int d = 0; d < MAX_DIRECTIONS; ++d)
            {
                const auto& index = indices.directions[static_cast<std::size_t>(a * MAX_DIRECTIONS + d)];
                if (index.size != 0)
                    dirs[static_cast<std::size_t>(d)] = loader.readMulAnimationFrames(indices.fileIndex, index, index.isVerdata);
            }
        }
        bool any = false;
        for (int d = 0; d < MAX_DIRECTIONS; ++d)
        {
            auto& list = dirs[static_cast<std::size_t>(d)];
            for (std::size_t f = 0; f < list.size() && f < 256; ++f)
            {
                if (list[f].empty())
                    continue;
                frames.emplace(frameId(a, d, static_cast<int>(f)), std::move(list[f]));
                any = true;
            }
        }
        r.actions += any ? 1 : 0;
    }
    if (frames.empty())
        return r;

    std::vector<Sprite> sprites;
    sprites.reserve(frames.size());
    const std::string prefix = "anim/" + bodyName(body) + "/";
    for (const auto& [id, f] : frames)
    {
        const int a = static_cast<int>(id >> 16), d = static_cast<int>((id >> 8) & 0xFF), n = static_cast<int>(id & 0xFF);
        Sprite s(id, f.width, f.height,
                 prefix + std::to_string(a) + "/" + std::to_string(d) + "/" + std::to_string(n));
        // ClassicUO draws a frame at (x - centerX, y - centerY - height), x/y being the mobile's
        // feet. As an Axmol anchor (bottom-left origin) that point is:
        s.hasAnchor = true;
        s.ax        = static_cast<float>(f.centerX) / static_cast<float>(f.width);
        s.ay        = -static_cast<float>(f.centerY) / static_cast<float>(f.height);
        s.extraJson = "\"action\":" + std::to_string(a) + ",\"dir\":" + std::to_string(d) + ",\"frame\":" +
                      std::to_string(n) + ",\"center\":[" + std::to_string(f.centerX) + "," + std::to_string(f.centerY) + "]";
        sprites.push_back(std::move(s));
    }
    auto decode = [&](const Sprite& s) {
        const FrameInfo& f = frames.at(s.id);
        uo::assets::Image img;
        img.width  = f.width;
        img.height = f.height;
        img.pixels = f.pixels;
        return img;
    };
    r.indices = std::move(indices);
    r.atlas   = writeAtlas(dir, bodyName(body), sprites, decode, options);
    return r;
}

const char* typeName(AnimationGroupsType t)
{
    switch (t)
    {
        case AnimationGroupsType::Monster: return "monster";
        case AnimationGroupsType::SeaMonster: return "seaMonster";
        case AnimationGroupsType::Animal: return "animal";
        case AnimationGroupsType::Human: return "human";
        case AnimationGroupsType::Equipment: return "equipment";
        default: return "unknown";
    }
}

}  // namespace

bool runAnims(Context& ctx, JsonWriter& m)
{
    uo::assets::Verdata verdata;
    if (ctx.opt.useVerdata)
    {
        if (std::string p = ctx.data.find("verdata.mul"); !p.empty())
            verdata.load(p);
    }
    AnimationsConfig cfg;
    cfg.directory         = ctx.opt.uoDir;
    cfg.version           = ctx.opt.clientVersion;
    cfg.isUopInstallation = ctx.opt.useUop && !ctx.data.find("AnimationFrame1.uop").empty();
    cfg.verdata           = verdata.isOpen() ? &verdata : nullptr;
    cfg.resolve           = [&ctx](const std::string& name) { return ctx.data.find(name); };

    AnimationsLoader loader;
    if (!loader.load(cfg))
    {
        m.field("skipped", "no anim.mul/anim.idx or AnimationFrame*.uop");
        return false;
    }

    // Body.def, Bodyconv.def and friends are left to the client: they remap bodies at runtime
    // (Bodyconv.def even depends on the server's expansion flags), so every body is exported
    // as stored.
    std::vector<std::pair<std::uint32_t, AnimationsLoader::Indices>> bodies;
    for (std::uint32_t body = 0; body < 0xFFFFu; ++body)
    {
        AnimationsLoader::Indices idx = loader.getIndices(static_cast<std::uint16_t>(body));
        bool any = std::any_of(idx.directions.begin(), idx.directions.end(),
                               [](const AnimationDirectionIndex& d) { return d.size != 0; });
        if (any)
            bodies.emplace_back(body, std::move(idx));
    }

    const std::string dir = ctx.outDir("anims");
    AtlasOptions options;
    options.maxSize = ctx.opt.maxSize;
    options.jobs    = 1;  // bodies run in parallel instead

    std::vector<BodyResult> results(bodies.size());
    std::atomic<std::size_t> next{0};
    auto worker = [&] {
        for (std::size_t i; (i = next++) < bodies.size();)
            results[i] = convertBody(loader, bodies[i].first, std::move(bodies[i].second), dir, options);
    };
    int jobs = ctx.opt.jobs > 0 ? ctx.opt.jobs : static_cast<int>(std::max(1u, std::thread::hardware_concurrency()));
    jobs     = std::max(1, std::min<int>(jobs, static_cast<int>(bodies.size())));
    std::vector<std::thread> threads;
    for (int t = 1; t < jobs; ++t)
        threads.emplace_back(worker);
    worker();
    for (auto& t : threads)
        t.join();

    std::size_t frames = 0, sheets = 0, written = 0, undecodable = 0;
    JsonWriter j;
    j.beginObject().field("version", 1).key("bodies").beginObject();
    for (const BodyResult& r : results)
    {
        if (r.atlas.frames == 0)
            continue;
        ++written;
        frames += r.atlas.frames;
        sheets += r.atlas.sheets;
        undecodable += r.atlas.missing.size();
        j.key(std::to_string(r.body)).beginObject();
        j.field("index", bodyName(r.body) + ".json").field("type", typeName(r.indices.type));
        j.field("uop", (r.indices.flags & AnimationFlags::UseUopAnimation) != 0);
        j.field("file", static_cast<std::int64_t>(r.indices.fileIndex));
        j.field("flags", static_cast<std::uint64_t>(r.indices.flags));
        j.field("actions", static_cast<std::int64_t>(r.actions)).field("frames", static_cast<std::uint64_t>(r.atlas.frames));
        j.endObject();
    }
    j.endObject().endObject();
    j.save((fs::path(dir) / "anims.json").string());

    m.field("bodies", static_cast<std::uint64_t>(written)).field("frames", static_cast<std::uint64_t>(frames));
    m.field("sheets", static_cast<std::uint64_t>(sheets));
    if (undecodable)
        m.field("undecodable", static_cast<std::uint64_t>(undecodable));
    return true;
}

}  // namespace uoconvert
