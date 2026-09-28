// SPDX-License-Identifier: BSD-2-Clause
// Bitmap fonts as AngelCode BMFont (.fnt text + one .png page), which ax::Label::createWithBMFont
// reads. Glyphs are drawn by uo::text::FontRenderer, so baseline offsets and advances match the
// client's own text rendering.
#include "Atlas.h"
#include "Png.h"
#include "Stages.h"

#include "uo/text/FontRenderer.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <functional>

namespace fs = std::filesystem;

namespace uoconvert
{

namespace
{

using uo::text::FontRenderer;
using uo::text::TextAlign;
using uo::text::TextBitmap;

struct Glyph
{
    char16_t code = 0;
    int xoffset = 0, yoffset = 0, xadvance = 0;
    uo::assets::Image image;  // trimmed to its opaque pixels; empty for blank glyphs
};

// Crops a rendered single-character bitmap to its opaque pixels, keeping the offset.
Glyph trim(char16_t code, const TextBitmap& bmp, int advance, bool white)
{
    Glyph g;
    g.code     = code;
    g.xadvance = advance;
    if (bmp.empty())
        return g;
    int x0 = bmp.width, y0 = bmp.height, x1 = -1, y1 = -1;
    for (int y = 0; y < bmp.height; ++y)
        for (int x = 0; x < bmp.width; ++x)
            if (bmp.pixels[static_cast<std::size_t>(y) * bmp.width + x] != 0)
            {
                x0 = std::min(x0, x), x1 = std::max(x1, x);
                y0 = std::min(y0, y), y1 = std::max(y1, y);
            }
    if (x1 < 0)
        return g;
    g.xoffset      = x0;
    g.yoffset      = y0;
    g.image.width  = x1 - x0 + 1;
    g.image.height = y1 - y0 + 1;
    g.image.pixels.resize(static_cast<std::size_t>(g.image.width) * g.image.height);
    for (int y = 0; y < g.image.height; ++y)
        for (int x = 0; x < g.image.width; ++x)
        {
            std::uint32_t p = bmp.pixels[static_cast<std::size_t>(y + y0) * bmp.width + x + x0];
            // Unicode fonts are one bit per pixel: white lets Label::setTextColor tint them.
            g.image.pixels[static_cast<std::size_t>(y) * g.image.width + x] = (white && p) ? 0xFFFFFFFFu : p;
        }
    return g;
}

// Packs `glyphs` onto one page (Axmol's BMFont reader takes a single page) and writes
// <dir>/<name>.fnt and <dir>/<name>.png. Glyphs that do not fit are dropped and counted.
std::size_t writeBmFont(const std::string& dir, const std::string& name, const std::vector<Glyph>& glyphs,
                        int lineHeight, int maxSize, std::size_t& dropped)
{
    std::vector<Sprite> sprites;
    for (std::size_t i = 0; i < glyphs.size(); ++i)
        if (!glyphs[i].image.empty())
            sprites.emplace_back(static_cast<std::uint32_t>(i), glyphs[i].image.width, glyphs[i].image.height, "");
    AtlasOptions options;
    options.maxSize = maxSize;
    std::vector<Page> pages = planPages(sprites, options);

    std::vector<int> px(glyphs.size(), -1), py(glyphs.size(), -1);
    int w = 4, h = 4;
    if (!pages.empty())
    {
        w = pages[0].w, h = pages[0].h;
        for (const auto& pl : pages[0].placements)
            px[sprites[pl.sprite].id] = pl.x, py[sprites[pl.sprite].id] = pl.y;
        for (std::size_t p = 1; p < pages.size(); ++p)
            dropped += pages[p].placements.size();
    }
    std::vector<std::uint32_t> canvas(static_cast<std::size_t>(w) * h, 0);
    for (std::size_t i = 0; i < glyphs.size(); ++i)
    {
        if (px[i] < 0)
            continue;
        const auto& img = glyphs[i].image;
        for (int y = 0; y < img.height; ++y)
            std::copy_n(img.pixels.begin() + static_cast<std::ptrdiff_t>(y) * img.width, img.width,
                        canvas.begin() + static_cast<std::ptrdiff_t>(py[i] + y) * w + px[i]);
    }
    writePng((fs::path(dir) / (name + ".png")).string(), w, h, canvas.data());

    std::size_t count = 0;
    std::string chars;
    for (std::size_t i = 0; i < glyphs.size(); ++i)
    {
        const Glyph& g = glyphs[i];
        const bool placed = px[i] >= 0;
        if (!placed && !g.image.empty())
            continue;  // did not fit on the page
        chars += "char id=" + std::to_string(static_cast<unsigned>(g.code)) + " x=" + std::to_string(placed ? px[i] : 0) +
                 " y=" + std::to_string(placed ? py[i] : 0) + " width=" + std::to_string(g.image.width) +
                 " height=" + std::to_string(g.image.height) + " xoffset=" + std::to_string(g.xoffset) +
                 " yoffset=" + std::to_string(g.yoffset) + " xadvance=" + std::to_string(g.xadvance) +
                 " page=0 chnl=15\n";
        ++count;
    }
    std::ofstream f(fs::path(dir) / (name + ".fnt"), std::ios::binary);
    f << "info face=\"" << name << "\" size=" << lineHeight
      << " bold=0 italic=0 charset=\"\" unicode=1 stretchH=100 smooth=0 aa=1 padding=0,0,0,0 spacing=1,1\n";
    f << "common lineHeight=" << lineHeight << " base=" << lineHeight << " scaleW=" << w << " scaleH=" << h
      << " pages=1 packed=0\n";
    f << "page id=0 file=\"" << name << ".png\"\n";
    f << "chars count=" << count << "\n" << chars;
    return count;
}

}  // namespace

bool runFonts(Context& ctx, JsonWriter& m)
{
    FontRenderer fonts;
    int asciiCount = 0;
    if (std::string p = ctx.data.find("fonts.mul"); !p.empty())
    {
        std::vector<std::uint8_t> data = Context::readFile(p);
        asciiCount                     = fonts.loadAsciiFonts(data);
    }
    std::vector<int> unicode;
    for (int i = 0; i < FontRenderer::kMaxUnicodeFonts; ++i)
    {
        std::string p = ctx.data.find(i == 0 ? "unifont.mul" : "unifont" + std::to_string(i) + ".mul");
        if (!p.empty() && fonts.setUnicodeFont(i, p))
            unicode.push_back(i);
    }
    if (asciiCount == 0 && unicode.empty())
    {
        m.field("skipped", "no fonts.mul or unifont*.mul");
        return false;
    }

    const std::string dir = ctx.outDir("fonts");
    const int maxSize     = std::max(ctx.opt.maxSize, 4096);
    std::size_t glyphs = 0, dropped = 0;
    JsonWriter j;
    j.beginObject().field("version", 1).key("fonts").beginArray();
    auto emit = [&](const std::string& name, const char* kind, int index, std::vector<Glyph>& list, int lineHeight) {
        glyphs += writeBmFont(dir, name, list, lineHeight, maxSize, dropped);
        j.beginObject().field("name", name).field("kind", kind).field("index", static_cast<std::int64_t>(index));
        j.field("fnt", name + ".fnt").field("lineHeight", static_cast<std::int64_t>(lineHeight)).endObject();
    };

    // fonts.mul: 224 pre-coloured glyphs per font, for character codes 32..255 (the client
    // indexes them by the low byte, which is Latin-1).
    for (int f = 0; f < asciiCount; ++f)
    {
        std::vector<Glyph> list;
        int lineHeight = 0;
        for (char16_t c = 32; c < 256; ++c)
        {
            TextBitmap bmp = fonts.generateAscii(static_cast<std::uint8_t>(f), std::u16string(1, c), 0, 0, TextAlign::Left, 0);
            lineHeight     = std::max(lineHeight, bmp.height);
            list.push_back(trim(c, bmp, fonts.charWidthAscii(static_cast<std::uint8_t>(f), c), false));
        }
        emit("ascii" + std::to_string(f), "ascii", f, list, lineHeight);
    }

    // unifont*.mul: one bit per pixel, every character the file carries.
    for (int f : unicode)
    {
        const auto font = static_cast<std::uint8_t>(f);
        std::vector<Glyph> list;
        int lineHeight = 0;
        for (std::uint32_t c = 1; c < 0x10000; ++c)
        {
            const auto ch = static_cast<char16_t>(c);
            if (ch == u'\r' || (!fonts.unicodeGlyph(font, ch).data && ch != u' '))
                continue;
            TextBitmap bmp = fonts.generateUnicode(font, std::u16string(1, ch), 0, 30, 0, TextAlign::Left, 0);
            lineHeight     = std::max(lineHeight, bmp.height);
            list.push_back(trim(ch, bmp, fonts.charWidthUnicode(font, ch), true));
        }
        emit("unifont" + std::to_string(f), "unicode", f, list, lineHeight);
    }
    j.endArray().endObject();
    j.save((fs::path(dir) / "fonts.json").string());

    m.field("ascii", static_cast<std::int64_t>(asciiCount)).field("unicode", static_cast<std::uint64_t>(unicode.size()));
    m.field("glyphs", static_cast<std::uint64_t>(glyphs));
    if (dropped)
    {
        m.field("dropped", static_cast<std::uint64_t>(dropped));
        ctx.warn("fonts: " + std::to_string(dropped) + " glyphs did not fit one " + std::to_string(maxSize) + " px page");
    }
    return true;
}

}  // namespace uoconvert
