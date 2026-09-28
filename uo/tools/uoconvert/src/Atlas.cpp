// SPDX-License-Identifier: BSD-2-Clause
#include "Atlas.h"

#include "Json.h"
#include "Png.h"

#include <algorithm>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <optional>
#include <thread>
#include <unordered_set>

namespace fs = std::filesystem;

namespace uoconvert
{

namespace
{

class Skyline
{
public:
    Skyline(int w, int h) : _w(w), _h(h) { _segs.push_back({0, 0, w}); }

    std::optional<std::pair<int, int>> insert(int w, int h)
    {
        int bestTop = 0, bestX = 0, bestY = 0;
        std::size_t best = _segs.size();
        for (std::size_t i = 0; i < _segs.size(); ++i)
        {
            int y = fit(i, w, h);
            if (y < 0)
                continue;
            if (best == _segs.size() || y + h < bestTop || (y + h == bestTop && _segs[i].x < bestX))
            {
                best    = i;
                bestTop = y + h;
                bestX   = _segs[i].x;
                bestY   = y;
            }
        }
        if (best == _segs.size())
            return std::nullopt;

        _segs.insert(_segs.begin() + best, Seg{bestX, bestY + h, w});
        // Trim the segments the new one now covers.
        for (std::size_t k = best + 1; k < _segs.size();)
        {
            int prevEnd = _segs[k - 1].x + _segs[k - 1].w;
            if (_segs[k].x >= prevEnd)
                break;
            int shrink = prevEnd - _segs[k].x;
            _segs[k].x += shrink;
            _segs[k].w -= shrink;
            if (_segs[k].w > 0)
                break;
            _segs.erase(_segs.begin() + k);
        }
        for (std::size_t k = 0; k + 1 < _segs.size();)
        {
            if (_segs[k].y == _segs[k + 1].y)
            {
                _segs[k].w += _segs[k + 1].w;
                _segs.erase(_segs.begin() + k + 1);
            }
            else
                ++k;
        }
        usedW = std::max(usedW, bestX + w);
        usedH = std::max(usedH, bestY + h);
        return std::make_pair(bestX, bestY);
    }

    int usedW = 0;
    int usedH = 0;

private:
    struct Seg
    {
        int x, y, w;
    };

    int fit(std::size_t i, int w, int h) const
    {
        int x = _segs[i].x;
        if (x + w > _w)
            return -1;
        int y = 0, left = w;
        for (std::size_t j = i; left > 0; ++j)
        {
            if (j >= _segs.size())
                return -1;
            y = std::max(y, _segs[j].y);
            if (y + h > _h)
                return -1;
            left -= _segs[j].w;
        }
        return y;
    }

    int _w, _h;
    std::vector<Seg> _segs;
};

int round4(int v)
{
    return (v + 3) & ~3;
}

std::string pageName(const std::string& base, std::size_t n, const char* ext)
{
    char buf[16];
    std::snprintf(buf, sizeof(buf), "-%03zu.%s", n, ext);
    return base + buf;
}

std::string xmlEscape(const std::string& s)
{
    std::string out;
    for (char c : s)
    {
        switch (c)
        {
        case '&': out += "&amp;"; break;
        case '<': out += "&lt;"; break;
        case '>': out += "&gt;"; break;
        default: out += c;
        }
    }
    return out;
}

std::string fmtFloat(float f)
{
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.6g", f);
    return buf;
}

bool writePlist(const std::string& path, const std::string& png, const Page& page, const std::vector<Sprite>& sprites,
                const std::unordered_set<std::uint32_t>& missing)
{
    std::vector<const Placement*> order;
    for (const auto& p : page.placements)
        if (!missing.count(sprites[p.sprite].id))
            order.push_back(&p);
    std::sort(order.begin(), order.end(),
              [&](const Placement* a, const Placement* b) { return sprites[a->sprite].name < sprites[b->sprite].name; });

    std::string o;
    o += "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
         "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
         "<plist version=\"1.0\">\n<dict>\n\t<key>frames</key>\n\t<dict>\n";
    for (const Placement* p : order)
    {
        const Sprite& s  = sprites[p->sprite];
        std::string size = "{" + std::to_string(s.w) + "," + std::to_string(s.h) + "}";
        o += "\t\t<key>" + xmlEscape(s.name) + "</key>\n\t\t<dict>\n";
        if (s.hasAnchor)
            o += "\t\t\t<key>anchor</key>\n\t\t\t<string>{" + fmtFloat(s.ax) + "," + fmtFloat(s.ay) + "}</string>\n";
        o += "\t\t\t<key>aliases</key>\n\t\t\t<array/>\n";
        o += "\t\t\t<key>spriteOffset</key>\n\t\t\t<string>{0,0}</string>\n";
        o += "\t\t\t<key>spriteSize</key>\n\t\t\t<string>" + size + "</string>\n";
        o += "\t\t\t<key>spriteSourceSize</key>\n\t\t\t<string>" + size + "</string>\n";
        o += "\t\t\t<key>textureRect</key>\n\t\t\t<string>{{" + std::to_string(p->x) + "," + std::to_string(p->y) +
             "}," + size + "}</string>\n";
        o += "\t\t\t<key>textureRotated</key>\n\t\t\t<false/>\n\t\t</dict>\n";
    }
    o += "\t</dict>\n\t<key>metadata</key>\n\t<dict>\n";
    o += "\t\t<key>format</key>\n\t\t<integer>3</integer>\n";
    o += "\t\t<key>pixelFormat</key>\n\t\t<string>RGBA8888</string>\n";
    o += "\t\t<key>premultiplyAlpha</key>\n\t\t<false/>\n";
    o += "\t\t<key>realTextureFileName</key>\n\t\t<string>" + xmlEscape(png) + "</string>\n";
    o += "\t\t<key>size</key>\n\t\t<string>{" + std::to_string(page.w) + "," + std::to_string(page.h) + "}</string>\n";
    o += "\t\t<key>textureFileName</key>\n\t\t<string>" + xmlEscape(png) + "</string>\n";
    o += "\t</dict>\n</dict>\n</plist>\n";

    std::ofstream f(path, std::ios::binary);
    f << o;
    return static_cast<bool>(f);
}

void blit(std::vector<std::uint32_t>& canvas, int cw, const uo::assets::Image& img, int x, int y, int extrude)
{
    for (int yy = -extrude; yy < img.height + extrude; ++yy)
    {
        int sy = std::clamp(yy, 0, img.height - 1);
        std::uint32_t* dst = &canvas[static_cast<std::size_t>(y + yy) * cw + x - extrude];
        for (int xx = -extrude; xx < img.width + extrude; ++xx)
            *dst++ = img.at(std::clamp(xx, 0, img.width - 1), sy);
    }
}

}  // namespace

std::vector<Page> planPages(const std::vector<Sprite>& sprites, const AtlasOptions& options)
{
    const int border = options.extrude * 2 + options.padding;
    std::vector<std::size_t> order(sprites.size());
    for (std::size_t i = 0; i < order.size(); ++i)
        order[i] = i;
    std::stable_sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) {
        if (sprites[a].h != sprites[b].h)
            return sprites[a].h > sprites[b].h;
        if (sprites[a].w != sprites[b].w)
            return sprites[a].w > sprites[b].w;
        return sprites[a].id < sprites[b].id;
    });

    std::vector<Page> pages;
    std::vector<std::optional<Skyline>> packers;
    for (std::size_t i : order)
    {
        const Sprite& s = sprites[i];
        int w = s.w + border, h = s.h + border;
        if (w > options.maxSize || h > options.maxSize)
        {
            Page p{round4(s.w + options.extrude * 2), round4(s.h + options.extrude * 2), {}};
            p.placements.push_back({i, options.extrude, options.extrude});
            pages.push_back(std::move(p));
            packers.emplace_back(std::nullopt);
            continue;
        }
        bool placed = false;
        // Only the two newest pages are tried; older ones are nearly full, and scanning every
        // page makes a 60,000-sprite archive quadratic.
        for (std::size_t p = pages.size() >= 2 ? pages.size() - 2 : 0; p < pages.size() && !placed; ++p)
        {
            if (!packers[p])
                continue;
            if (auto pos = packers[p]->insert(w, h))
            {
                pages[p].placements.push_back({i, pos->first + options.extrude, pos->second + options.extrude});
                placed = true;
            }
        }
        if (!placed)
        {
            Skyline sk(options.maxSize, options.maxSize);
            auto pos = sk.insert(w, h);
            Page p{options.maxSize, options.maxSize, {}};
            p.placements.push_back({i, pos->first + options.extrude, pos->second + options.extrude});
            pages.push_back(std::move(p));
            packers.emplace_back(std::move(sk));
        }
    }
    for (std::size_t p = 0; p < pages.size(); ++p)
    {
        if (packers[p])
        {
            pages[p].w = round4(packers[p]->usedW);
            pages[p].h = round4(packers[p]->usedH);
        }
    }
    return pages;
}

AtlasResult writeAtlas(const std::string& dir, const std::string& base, const std::vector<Sprite>& sprites,
                       const Decoder& decode, const AtlasOptions& options)
{
    fs::create_directories(dir);
    std::vector<Page> pages = planPages(sprites, options);

    std::mutex lock;
    std::unordered_set<std::uint32_t> missing;
    std::atomic<std::size_t> next{0};

    auto worker = [&] {
        for (std::size_t n; (n = next++) < pages.size();)
        {
            const Page& page = pages[n];
            std::vector<std::uint32_t> canvas(static_cast<std::size_t>(page.w) * page.h, 0);
            std::vector<std::uint32_t> pageMissing;
            for (const auto& pl : page.placements)
            {
                const Sprite& s = sprites[pl.sprite];
                uo::assets::Image img = decode(s);
                if (img.empty() || img.width != s.w || img.height != s.h)
                {
                    pageMissing.push_back(s.id);
                    continue;
                }
                blit(canvas, page.w, img, pl.x, pl.y, options.extrude);
            }
            std::string png = pageName(base, n, "png");
            writePng((fs::path(dir) / png).string(), page.w, page.h, canvas.data());
            std::unordered_set<std::uint32_t> skip(pageMissing.begin(), pageMissing.end());
            writePlist((fs::path(dir) / pageName(base, n, "plist")).string(), png, page, sprites, skip);
            std::lock_guard g(lock);
            missing.insert(pageMissing.begin(), pageMissing.end());
        }
    };
    int jobs = std::max(1, std::min<int>(options.jobs, static_cast<int>(pages.size())));
    std::vector<std::thread> threads;
    for (int t = 1; t < jobs; ++t)
        threads.emplace_back(worker);
    worker();
    for (auto& t : threads)
        t.join();

    // Index: id -> sheet and rect, so the client can load a sheet on first use.
    struct Row
    {
        std::uint32_t id;
        std::size_t sheet;
        const Placement* pl;
    };
    std::vector<Row> rows;
    for (std::size_t n = 0; n < pages.size(); ++n)
        for (const auto& pl : pages[n].placements)
            if (!missing.count(sprites[pl.sprite].id))
                rows.push_back({sprites[pl.sprite].id, n, &pl});
    std::sort(rows.begin(), rows.end(), [](const Row& a, const Row& b) { return a.id < b.id; });

    JsonWriter j;
    j.beginObject().field("version", 1).key("sheets").beginArray();
    for (std::size_t n = 0; n < pages.size(); ++n)
        j.value(pageName(base, n, "plist"));
    j.endArray().field("count", static_cast<std::uint64_t>(rows.size())).key("frames").beginObject();
    for (const Row& r : rows)
    {
        const Sprite& s = sprites[r.pl->sprite];
        j.key(std::to_string(r.id)).beginObject();
        j.field("sheet", static_cast<std::uint64_t>(r.sheet));
        j.key("rect").beginArray().value(r.pl->x).value(r.pl->y).value(s.w).value(s.h).endArray();
        j.field("name", s.name);
        if (s.hasAnchor)
            j.key("anchor").beginArray().value(static_cast<double>(s.ax)).value(static_cast<double>(s.ay)).endArray();
        if (!s.extraJson.empty())
            j.raw(s.extraJson);  // already "key":value pairs
        j.endObject();
    }
    j.endObject();
    if (!options.indexExtraJson.empty())
        j.raw(options.indexExtraJson);
    j.endObject();
    j.save((fs::path(dir) / (base + ".json")).string());

    AtlasResult result;
    result.frames = rows.size();
    result.sheets = pages.size();
    result.missing.assign(missing.begin(), missing.end());
    std::sort(result.missing.begin(), result.missing.end());
    return result;
}

}  // namespace uoconvert
