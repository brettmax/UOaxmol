// SPDX-License-Identifier: BSD-2-Clause
// Data tables: tiledata, multis and animdata as JSON, decoded by uocore.
#include "Stages.h"

#include "uo/assets/AnimData.h"
#include "uo/assets/Multis.h"
#include "uo/assets/TileData.h"

#include <cctype>
#include <fstream>
#include <regex>

using uo::io::Verdata;

namespace uoconvert
{

std::vector<DefLine> readDef(const std::string& path)
{
    std::vector<DefLine> out;
    if (path.empty())
        return out;
    std::ifstream f(path);
    std::string line;
    auto number = [](const std::string& t, long& v) {
        try
        {
            std::size_t used = 0;
            v = (t.size() > 2 && t[0] == '0' && (t[1] == 'x' || t[1] == 'X')) ? std::stol(t.substr(2), &used, 16)
                                                                                : std::stol(t, &used, 10);
            return used > 0;
        }
        catch (...)
        {
            return false;
        }
    };
    auto numbers = [&](const std::string& s, std::vector<long>& into) {
        static const std::regex tok("[^\\s,{}]+");
        for (auto it = std::sregex_iterator(s.begin(), s.end(), tok); it != std::sregex_iterator(); ++it)
        {
            long v;
            if (number(it->str(), v))
                into.push_back(v);
        }
    };
    while (std::getline(f, line))
    {
        std::size_t start = line.find_first_not_of(" \t\r");
        if (start == std::string::npos || !std::isdigit(static_cast<unsigned char>(line[start])))
            continue;
        line = line.substr(start, line.find('#') == std::string::npos ? std::string::npos : line.find('#') - start);
        DefLine d;
        std::size_t open = line.find('{'), close = line.find('}');
        if (open != std::string::npos && close != std::string::npos && close > open)
        {
            d.hasGroup = true;
            numbers(line.substr(0, open), d.before);
            numbers(line.substr(open, close - open + 1), d.group);
            numbers(line.substr(close + 1), d.after);
        }
        else
            numbers(line, d.before);
        out.push_back(std::move(d));
    }
    return out;
}

bool runTileData(Context& ctx, JsonWriter& m)
{
    using uo::assets::TileData;
    std::string path = ctx.data.find("tiledata.mul");
    if (path.empty())
    {
        m.field("skipped", "no tiledata.mul");
        return false;
    }
    std::vector<std::uint8_t> bytes = Context::readFile(path);

    // Verdata FileID 30 replaces whole 32-tile groups: 836-byte land groups for blocks below
    // 0x200, 1188-byte static groups above (the pre-High Seas layout, as in UOFileManager).
    std::size_t patched = 0;
    if (!ctx.newFormat)
    {
        constexpr std::size_t kLand = 4 + 32 * 26, kStatic = 4 + 32 * 37;
        for (const auto& p : ctx.verdata.patches())
        {
            if (p.fileId != Verdata::TileData)
                continue;
            auto src = ctx.verdata.bytes(p);
            std::size_t at;
            if (p.length == kLand && p.blockId < 0x200)
                at = p.blockId * kLand;
            else if (p.length == kStatic && p.blockId >= 0x200)
                at = 0x200 * kLand + (p.blockId - 0x200) * kStatic;
            else
                continue;
            if (src.size() == p.length && at + p.length <= bytes.size())
            {
                std::copy(src.begin(), src.end(), bytes.begin() + at);
                ++patched;
            }
        }
    }

    TileData td;
    if (!td.loadFromBytes(bytes, ctx.newFormat ? TileData::Format::New : TileData::Format::Old))
    {
        m.field("skipped", "tiledata.mul is malformed");
        return false;
    }

    JsonWriter j;
    j.beginObject().field("version", 1).field("newFormat", ctx.newFormat);
    j.field("landColumns", "flags, texId, name");
    j.field("staticColumns", "flags, weight, layer, count, animId, hue, lightIndex, height, name");
    j.key("land").beginArray();
    for (const auto& t : td.land())
        j.beginArray().value(t.flags).value(t.texId).value(latin1ToUtf8(t.name)).endArray();
    j.endArray().key("statics").beginArray();
    for (const auto& t : td.statics())
        j.beginArray()
            .value(t.flags)
            .value(t.weight)
            .value(t.layer)
            .value(t.count)
            .value(t.animId)
            .value(t.hue)
            .value(t.lightIndex)
            .value(t.height)
            .value(latin1ToUtf8(t.name))
            .endArray();
    j.endArray().endObject();
    j.save(ctx.out("data/tiledata.json"));

    m.field("land", static_cast<std::uint64_t>(td.land().size()));
    m.field("statics", static_cast<std::uint64_t>(td.statics().size()));
    m.field("verdataPatches", static_cast<std::uint64_t>(patched));
    return true;
}

bool runMultis(Context& ctx, JsonWriter& m)
{
    auto file = ctx.openIndexed("", "", "multi.mul", "multi.idx");
    if (!file)
    {
        m.field("skipped", "no multi.mul/multi.idx (MultiCollection.uop is not read yet)");
        return false;
    }
    JsonWriter j;
    j.beginObject().field("version", 1).field("columns", "graphic, x, y, z, flags");
    j.key("multis").beginObject();
    std::size_t count = 0;
    for (std::uint32_t id = 0; id < file->count(); ++id)
    {
        EntryBytes e = ctx.entry(file.get(), id, Verdata::Multi);
        if (e.bytes.empty())
            continue;
        auto parts = uo::assets::Multis::decode(e.bytes, ctx.newFormat);
        if (parts.empty())
            continue;
        j.key(std::to_string(id)).beginArray();
        for (const auto& p : parts)
            j.beginArray().value(p.graphic).value(p.x).value(p.y).value(p.z).value(p.flags).endArray();
        j.endArray();
        ++count;
    }
    j.endObject().endObject();
    j.save(ctx.out("data/multis.json"));
    m.field("multis", static_cast<std::uint64_t>(count));
    return true;
}

bool runAnimData(Context& ctx, JsonWriter& m)
{
    std::string path = ctx.data.find("animdata.mul");
    uo::assets::AnimData ad;
    if (path.empty() || !ad.load(path))
    {
        m.field("skipped", "no animdata.mul");
        return false;
    }
    JsonWriter j;
    j.beginObject().field("version", 1).field("columns", "frame offsets, frameInterval, frameStart");
    j.key("animations").beginObject();
    std::size_t count = 0;
    for (std::uint32_t g = 0; g < ad.count(); ++g)
    {
        const auto* e = ad.get(g);
        if (!e->frameCount)
            continue;
        j.key(std::to_string(g)).beginArray().beginArray();
        for (int f = 0; f < e->frameCount && f < 64; ++f)
            j.value(static_cast<int>(e->frames[f]));
        j.endArray().value(e->frameInterval).value(e->frameStart).endArray();
        ++count;
    }
    j.endObject().endObject();
    j.save(ctx.out("data/animdata.json"));
    m.field("animated", static_cast<std::uint64_t>(count));
    return true;
}

}  // namespace uoconvert
