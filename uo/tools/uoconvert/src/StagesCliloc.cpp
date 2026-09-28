// SPDX-License-Identifier: BSD-2-Clause
// Cliloc tables as JSON, one per language, decoded by uo::assets::Cliloc (plain or BWT files).
#include "Stages.h"

#include "uo/assets/Cliloc.h"

#include <algorithm>
#include <filesystem>

namespace fs = std::filesystem;

namespace uoconvert
{

bool runCliloc(Context& ctx, JsonWriter& m)
{
    // Cliloc.<lang> files at the top of the client folder.
    std::vector<std::pair<std::string, std::string>> langs;  // lang (lower case), path
    for (const std::string& rel : ctx.data.list("", ""))
    {
        std::string lower = toLower(rel);
        if (lower.find('/') == std::string::npos && lower.rfind("cliloc.", 0) == 0 && lower.size() > 7)
            langs.emplace_back(lower.substr(7), (fs::path(ctx.data.root()) / rel).string());
    }
    if (langs.empty())
    {
        m.field("skipped", "no Cliloc.* files");
        return false;
    }
    const std::string enu    = ctx.data.find("Cliloc.enu");
    const std::string custom = ctx.data.find("Clilocs.txt");
    std::string overrides;
    if (!custom.empty())
    {
        std::vector<std::uint8_t> text = Context::readFile(custom);
        overrides.assign(text.begin(), text.end());
    }

    m.key("languages").beginObject();
    for (const auto& [lang, path] : langs)
    {
        // Layered as the client loads them: Cliloc.enu under another language, Clilocs.txt on top.
        uo::assets::Cliloc cliloc;
        if (lang != "enu" && !enu.empty())
            cliloc.load(enu);
        if (!cliloc.load(path))
        {
            ctx.warn("cliloc: could not decode " + path);
            continue;
        }
        if (!overrides.empty())
            cliloc.loadOverrides(overrides);

        std::vector<std::pair<std::int32_t, const std::string*>> rows;
        rows.reserve(cliloc.entries().size());
        for (const auto& [number, text] : cliloc.entries())
            rows.emplace_back(number, &text);
        std::sort(rows.begin(), rows.end());

        JsonWriter j;
        j.beginObject().field("version", 1).field("language", lang).key("entries").beginObject();
        for (const auto& [number, text] : rows)
            j.field(std::to_string(number), *text);
        j.endObject().endObject();
        j.save(ctx.out("data/cliloc." + lang + ".json"));
        m.field(lang, static_cast<std::uint64_t>(rows.size()));
    }
    m.endObject();
    return true;
}

}  // namespace uoconvert
