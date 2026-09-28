// SPDX-License-Identifier: BSD-2-Clause
#include "TestUtil.h"
#include "TextFixture.h"
#include "doctest.h"

#include "uo/assets/Bwt.h"
#include "uo/assets/Cliloc.h"
#include "uo/text/FontRenderer.h"
#include "uo/text/JournalText.h"
#include "uo/text/SpeechText.h"
#include "uo/text/Utf.h"
#include "uo/text/WorldClilocs.h"

#include <cstdio>
#include <sstream>
#include <string>
#include <vector>

using namespace uo;
using namespace uotest;

namespace
{

std::string hex16(std::u16string_view s)
{
    if (s.empty())
        return "-";
    std::string out;
    char buf[8];
    for (char16_t c : s)
    {
        std::snprintf(buf, sizeof(buf), "%04X", static_cast<unsigned>(c));
        out += buf;
    }
    return out;
}

std::string hexU(std::uint64_t v)
{
    char buf[24];
    std::snprintf(buf, sizeof(buf), "%llX", static_cast<unsigned long long>(v));
    return buf;
}

std::string hexPadded(std::uint64_t v)
{
    char buf[24];
    std::snprintf(buf, sizeof(buf), "%016llX", static_cast<unsigned long long>(v));
    return buf;
}

std::uint64_t fnvBytes(std::string_view s)
{
    std::uint64_t h = 1469598103934665603ULL;
    for (unsigned char c : s)
    {
        h ^= c;
        h *= 1099511628211ULL;
    }
    return h;
}

std::uint64_t fnvPixels(const std::vector<std::uint32_t>& d)
{
    std::uint64_t h = 1469598103934665603ULL;
    for (std::uint32_t v : d)
        for (int i = 0; i < 4; i++)
        {
            h ^= static_cast<std::uint8_t>(v >> (i * 8));
            h *= 1099511628211ULL;
        }
    return h;
}

std::string layoutString(const text::TextLayout& layout)
{
    std::ostringstream o;
    for (const auto& line : layout)
    {
        o << '[' << static_cast<int>(line.align) << ',' << line.charCount << ',' << line.charStart << ','
          << line.indentionOffset << ',' << line.maxHeight << ',' << line.width << ':';
        for (const auto& d : line.data)
            o << std::uppercase << std::hex << static_cast<unsigned>(d.item) << '/' << d.color << '/' << d.flags << '/'
              << std::dec << static_cast<int>(d.font) << ',';
        o << ']';
    }
    return o.str();
}

struct FixtureHues : text::HueResolver
{
    std::vector<std::uint16_t> table;  // 32 entries per hue

    FixtureHues()
    {
        Bytes b = textfixture::hueTables();
        for (std::size_t i = 4; i + 1 < b.size(); i += 2)
            table.push_back(static_cast<std::uint16_t>(b[i] | (b[i + 1] << 8)));
    }
    int hueCount() const override { return static_cast<int>(table.size() / 32); }
    std::uint16_t colorTableEntry(std::uint16_t hue, int index) const override { return table[(hue - 1) * 32 + index]; }
};

struct Fixture
{
    FixtureHues hues;
    text::FontRenderer fonts;
    assets::Cliloc cliloc;

    Fixture()
    {
        fonts.setHueResolver(&hues);
        fonts.loadAsciiFonts(textfixture::fontsMul());
        fonts.setUnicodeFont(0, textfixture::unifontMul());
        cliloc.loadFromBytes(textfixture::clilocEnu());
        cliloc.loadOverrides(textfixture::clilocsTxt());
    }
};

struct RenderCase
{
    char kind;
    int font, flags, width, align, hue, cell, height;
    const char* text;
};

// Same inputs the oracle ran; ASCII strings are single bytes, unicode strings UTF-8.
const char* kLong = "The quick brown fox jumps over the lazy dog near the river bank";
const RenderCase kRenderCases[] = {
    {'A', 1, 0, 0, 0, 0, 30, 0, "Hello World"},
    {'A', 3, 0x8, 120, 0, 0x0021, 30, 0, kLong},
    {'A', 4, 0, 90, 1, 5, 30, 0, kLong},
    {'A', 6, 0, 70, 2, 0, 30, 0, "line one\nline two\n"},
    {'A', 9, 0x4, 80, 0, 0xFFFF, 30, 0, kLong},
    {'A', 0, 0x40, 60, 0, 3, 30, 0, "Averyveryverylongsingleword"},
    {'A', 2, 0x20, 60, 0, 7, 30, 0, kLong},
    {'A', 5, 0, 0, 0, 11, 30, 0, "partial \xE9\xFC hue"},
    {'A', 8, 0x200, 50, 0, 2, 30, 30, kLong},
    {'U', 0, 0, 0, 0, 0xFFFF, 30, 0, "Hello World"},
    {'U', 0, 0x8, 120, 0, 0x0021, 30, 0, kLong},
    {'U', 1, 0x8, 200, 1, 0x0035, 30, 0, kLong},
    {'U', 0, 0x1, 100, 0, 4, 30, 0, kLong},
    {'U', 0, 0x2, 100, 2, 4, 20, 0, kLong},
    {'U', 0, 0x10, 100, 0, 6, 30, 0, "under lined text"},
    {'U', 0, 0x1 | 0x8 | 0x2, 90, 1, 9, 30, 0, kLong},
    {'U', 0, 0x40, 70, 0, 0xFFFF, 30, 0, "Averyveryverylongsingleword"},
    {'U', 0, 0x20, 70, 0, 0xFFFF, 30, 0, kLong},
    {'U', 0, 0x100 | 0x4, 80, 0, 0xFFFF, 30, 0, kLong},
    {'U', 0, 0x200, 60, 0, 0xFFFF, 30, 40, kLong},
    {'U', 0, 0, 0, 0, 0xFFFF, 30, 0, "tab\there \r\n\xC5\x81\xC3\xA9 end\n"},
    {'U', 2, 0, 0, 0, 0xFFFF, 30, 0, "no such font"},
};

struct TranslateCase
{
    int number;
    bool capitalize;
    const char* args;
};

const TranslateCase kTranslateCases[] = {
    {500000, false, "Brett\t#1000"}, {500000, true, "brett the\t#1002"}, {500001, false, "12\t1000\tz"},
    {500001, false, "1000"},         {500000, false, "\tA\tB"},          {500000, false, "A\t"},
    {500002, false, "x"},            {500003, false, "x"},               {500004, false, "a"},
    {999999, false, ""},             {3000000, false, "v"},              {3100002, true, ""},
    {500000, false, "#-3\t#abc"},    {500000, false, " 1000 \t1000"},    {500005, true, ""},
    {500000, false, "#1001\tq"},
};

// Output of the original ClassicUO FontsLoader/ClilocLoader over the fixture: per render
// case w/h/x/c/t/l/g (width, height, wrapped width, cropped and uncropped text by width,
// layout, generated bitmap), then one r line per translate case. Layouts are FNV-1a hashed.
const char* const kExpected[] = {
    "w 70",
    "h 13",
    "x 9",
    "c 002E002E002E",
    "t -",
    "l #157CC9D938F5696A",
    "g 74 13 1 71A4B29B50A38740",
    "w 323",
    "h 50",
    "x 104",
    "c 00540068006500200071007500690063006B002000620072006F0077006E00200066006F002E002E002E",
    "t 00540068006500200071007500690063006B002000620072006F0077006E00200066006F00780020006A",
    "l #EDD40516C97E3605",
    "g 124 50 4 A8F1D7E5EADBA25E",
    "w 367",
    "h 76",
    "x 84",
    "c 00540068006500200071007500690063006B002000620072006F0077002E002E002E",
    "t 00540068006500200071007500690063006B002000620072006F0077006E00200066",
    "l #6AF70B48759FFF0A",
    "g 94 76 5 13B8EFB7656CEBBE",
    "w 103",
    "h 42",
    "x 49",
    "c 006C0069006E00650020006F006E002E002E002E",
    "t 006C0069006E00650020006F006E0065000A006C0069",
    "l #1AF7C45A0B9BF87D",
    "g 74 42 3 750CF41703AE993F",
    "w 326",
    "h 64",
    "x 80",
    "c 00540068006500200071007500690063006B00200062002E002E002E",
    "t 00540068006500200071007500690063006B002000620072006F0077006E00200066",
    "l #DF0BDBF678EB7710",
    "g 84 64 5 5880E5E5E74022AB",
    "w 140",
    "h 9",
    "x 114",
    "c 0041007600650072007900760065002E002E002E",
    "t 00410076006500720079007600650072007900760065",
    "l #8A45AE2AC96F488A",
    "g 64 9 1 28510422EA024AF0",
    "w 353",
    "h 13",
    "x 91",
    "c 00540068006500200071007500690063006B00200062002E002E002E",
    "t 00540068006500200071007500690063006B00200062",
    "l #F98F63DE3229F2F3",
    "g 64 13 1 E37891A6E93FB9",
    "w 74",
    "h 14",
    "x 10",
    "c 002E002E002E",
    "t -",
    "l #F61B40C8CCFE48F3",
    "g 78 14 1 FD0E7E1ED45AB96D",
    "w 367",
    "h 137",
    "x 49",
    "c 0054006800650020002E002E002E",
    "t 00540068006500200071007500690063006B",
    "l #65CD365672402160",
    "g 54 54 4 4DAC3EF705237F11",
    "w 73",
    "h 19",
    "x 17",
    "c 002E002E002E",
    "t -",
    "l #C50B6B6B136A1019",
    "g 77 23 1 46D93B1D994FFE7B",
    "w 510",
    "h 91",
    "x 114",
    "c 0054006800650020007500690063006B00200072006F002E002E002E",
    "t 0054006800650020007500690063006B00200072006F0077006E0020",
    "l #1A53939B20F186B0",
    "g 124 95 5 37670C896CD83343",
    "w 510",
    "h 55",
    "x 198",
    "c 0054006800650020007500690063006B00200072006F0077006E00200066006F00780020006A0075002E002E002E",
    "t 0054006800650020007500690063006B00200072006F0077006E00200066006F00780020006A0075006D00700073",
    "l #B47136E6D2666D35",
    "g 204 59 3 AD42BFB1C2858C3",
    "w 510",
    "h 106",
    "x 103",
    "c 0054006800650020007500690063006B0020002E002E002E",
    "t 0054006800650020007500690063006B00200072006F0077",
    "l #C5EEDEB40C25B49A",
    "g 104 110 6 6213B5B74324C6D8",
    "w 510",
    "h 106",
    "x 103",
    "c 0054006800650020007500690063006B0020002E002E002E",
    "t 0054006800650020007500690063006B00200072006F0077",
    "l #F9D7636B433FEAC4",
    "g 104 110 6 AD206FC14DC04323",
    "w 134",
    "h 37",
    "x 98",
    "c 0075006E00640065007200200069002E002E002E",
    "t 0075006E00640065007200200069006E00650064",
    "l #29697DD2D1F8D352",
    "g 104 41 2 DF0F6A60834009A8",
    "w 510",
    "h 135",
    "x 84",
    "c 0054006800650020007500690063006B002E002E002E",
    "t 0054006800650020007500690063006B00200072",
    "l #C51727E69B6E376F",
    "g 94 139 8 EB6687010132ECA0",
    "w 218",
    "h 18",
    "x 140",
    "c 0076006500720079002E002E002E",
    "t 00760065007200790076006500720079",
    "l #8AADFF431ABE7C86",
    "g 74 22 1 4F5DE5E3FDA75C14",
    "w 510",
    "h 18",
    "x 99",
    "c 00540068006500200075002E002E002E",
    "t 0054006800650020007500690063006B",
    "l #9F25C9E3C3E4079C",
    "g 74 22 1 F70D6F76FB6C03E4",
    "w 510",
    "h 199",
    "x 83",
    "c 0054006800650020007500690063002E002E002E",
    "t 0054006800650020007500690063006B00200072",
    "l #019254FC404E6C2D",
    "g 84 203 10 8AB7C88CC2D7FB0B",
    "w 510",
    "h 202",
    "x 61",
    "c 0054006800650020002E002E002E",
    "t 0054006800650020007500690063",
    "l #014BFC9B6A9E8EA0",
    "g 64 68 4 7E5072BC656E0CD4",
    "w 54",
    "h 65",
    "x 16",
    "c 002E002E002E",
    "t -",
    "l #CED22D8D328BF380",
    "g 58 69 4 7DEB4B85C9EEE203",
    "w 0",
    "h 0",
    "x 0",
    "c -",
    "t -",
    "l #14650FB0739D0383",
    "g 0 0 0 0",
    "r 0059006F00750020007300650065003A0020004200720065007400740020007700690074006800200061002000730077006F00720064",
    "r 0059006F00750020005300650065003A0020004200720065007400740020005400680065002000570069007400680020004800E9006C006C006F0020005700F60072006C0064",
    "r 0031003200200067006F006C00640020007A00200061006E006400200061002000730077006F00720064",
    "r 003100300030003000200067006F006C0064002000200061006E00640020",
    "r 0059006F00750020007300650065003A002000410020007700690074006800200042",
    "r 0059006F00750020007300650065003A00200041002000770069007400680020",
    "r 004D0065006700610043006C0069006C006F0063003A0020006500720072006F007200200066006F00720020003500300030003000300032",
    "r 0055006E0063006C006F0073006500640020007E0031005F004E0041004D0045",
    "r 00200069007300200061",
    "r 004D0065006700610043006C0069006C006F0063003A0020006D0069007300730069006E006700200039003900390039003900390020005B005D0020005B005D",
    "r 006F00760065007200720069006400650020007600200078",
    "r 00530065006E006400200054006F0020004B0065006E006E0065006C",
    "r 0059006F00750020007300650065003A0020004D0065006700610043006C0069006C006F0063003A0020006D0069007300730069006E00670020002D00330020005B007E0031005F00760061006C007E005D0020005B007E0032005F00760061006C007E005D0020007700690074006800200023006100620063",
    "r 0059006F00750020007300650065003A00200061002000730077006F007200640020007700690074006800200061002000730077006F00720064",
    "r 00540068006500200051007500690063006B0020002000420072006F0077006E00090046006F0078",
    "r 0059006F00750020007300650065003A00200020007700690074006800200071",
};

}  // namespace

TEST_CASE("font layout and rendering match the original client on the fixture")
{
    Fixture fx;
    std::vector<std::string> actual;

    for (const auto& c : kRenderCases)
    {
        std::u16string s;
        if (c.kind == 'A')
        {
            for (const char* p = c.text; *p; ++p)
                s.push_back(static_cast<unsigned char>(*p));
        }
        else
        {
            s = text::utf8ToUtf16(c.text);
        }

        const auto f  = static_cast<std::uint8_t>(c.font);
        const auto fl = static_cast<std::uint16_t>(c.flags);
        const auto al = static_cast<text::TextAlign>(c.align);
        const auto hue = static_cast<std::uint16_t>(c.hue);
        auto& r = fx.fonts;

        if (c.kind == 'A')
        {
            actual.push_back("w " + std::to_string(r.widthAscii(f, s)));
            actual.push_back("h " + std::to_string(r.heightAscii(f, s, c.width, al, fl)));
            actual.push_back("x " + std::to_string(r.widthExAscii(f, s, c.width, al, fl)));
            actual.push_back("c " + hex16(r.textByWidthAscii(f, s, c.width, true)));
            actual.push_back("t " + hex16(r.textByWidthAscii(f, s, c.width, false)));
            actual.push_back("l #" + hexPadded(fnvBytes(layoutString(r.layoutAscii(f, s, al, fl, c.width, true, true)))));
            auto g = r.generateAscii(f, s, hue, c.width, al, fl, c.height);
            actual.push_back("g " + std::to_string(g.width) + " " + std::to_string(g.height) + " " +
                             std::to_string(g.lineCount) + " " + hexU(g.pixels.empty() ? 0 : fnvPixels(g.pixels)));
        }
        else
        {
            actual.push_back("w " + std::to_string(r.widthUnicode(f, s)));
            actual.push_back("h " + std::to_string(r.heightUnicode(f, s, c.width, al, fl)));
            actual.push_back("x " + std::to_string(r.widthExUnicode(f, s, c.width, al, fl)));
            actual.push_back("c " + hex16(r.textByWidthUnicode(f, s, c.width, true)));
            actual.push_back("t " + hex16(r.textByWidthUnicode(f, s, c.width, false)));
            actual.push_back("l #" + hexPadded(fnvBytes(layoutString(r.layoutUnicode(f, s, al, fl, c.width, true, true)))));
            auto g = r.generateUnicode(f, s, hue, static_cast<std::uint8_t>(c.cell), c.width, al, fl, c.height);
            actual.push_back("g " + std::to_string(g.width) + " " + std::to_string(g.height) + " " +
                             std::to_string(g.lineCount) + " " + hexU(g.pixels.empty() ? 0 : fnvPixels(g.pixels)));
        }
    }

    for (const auto& c : kTranslateCases)
        actual.push_back("r " + hex16(text::utf8ToUtf16(fx.cliloc.translate(c.number, c.args, c.capitalize))));

    REQUIRE(actual.size() == std::size(kExpected));

    for (std::size_t i = 0; i < actual.size(); ++i)
    {
        CAPTURE(i);
        CHECK(actual[i] == kExpected[i]);
    }
}

TEST_CASE("cliloc translate follows the original argument rules")
{
    Fixture fx;
    auto& c = fx.cliloc;

    CHECK(c.translate(500000, "Brett\t#1000") == "You see: Brett with a sword");
    // A plain number is looked up as a cliloc only when there are several arguments.
    CHECK(c.translate(500001, "12\t1000\tz") == "12 gold z and a sword");
    CHECK(c.translate(500001, "1000") == "1000 gold  and ");
    // Leading tabs are skipped.
    CHECK(c.translate(500000, "\tA\tB") == "You see: A with B");
    CHECK(c.translate(500002, "x") == "MegaCliloc: error for 500002");
    CHECK(c.translate(500003, "x") == "Unclosed ~1_NAME");
    CHECK(c.translate(999999) == "MegaCliloc: missing 999999 [] []");
    CHECK(c.translate(500000, "brett the\t#1002", true) == "You See: Brett The With H\xC3\xA9llo W\xC3\xB6rld");
    // Clilocs.txt adds new numbers and overrides existing ones.
    CHECK(c.getString(3100001) == "Breed");
    CHECK(c.getString(3100002, {}, true) == "Send To Kennel");
    CHECK(c.translate(3000000, "v") == "override v x");
    CHECK(c.getString(424242, "fallback") == "fallback");
    CHECK(c.format(500000, "Brett\t#1000") == c.translate(500000, "Brett\t#1000"));
}

#ifdef UO_TEXT_HAS_WORLD_CLILOCS
TEST_CASE("world cliloc resolver reads through to the cliloc table")
{
    Fixture fx;
    text::WorldClilocs r(fx.cliloc);
    world::ClilocResolver& resolver = r;

    CHECK(resolver.get(1000) == "a sword");
    CHECK(resolver.get(999999).empty());
    CHECK(resolver.translate(500000, "Brett\t#1000", false) == std::optional<std::string>("You see: Brett with a sword"));
    CHECK(resolver.translate(999999, "", false) == std::optional<std::string>("MegaCliloc: missing 999999 [] []"));
    CHECK(resolver.translate(500000, "brett the\t#1002", true) ==
          std::optional<std::string>("You See: Brett The With H\xC3\xA9llo W\xC3\xB6rld"));
}
#endif

TEST_CASE("cliloc loads BWT-compressed files")
{
    // Build the decoded stream: 256 little-endian symbol counts, then per-symbol indices.
    // Only 'A' occurs, three times, so the output is "AAA".
    Bytes list(1024, 0);
    list['A' * 4] = 3;
    list.insert(list.end(), {0, 0, 0, 0});

    // Move-to-front encode it behind a 4-byte header whose byte 3 marks compression,
    // plus the trailing byte the decoder never reads.
    Bytes file = {0, 0, 0, 0x8E};
    std::vector<std::uint8_t> table(256);
    for (int i = 0; i < 256; ++i)
        table[i] = static_cast<std::uint8_t>(i);
    for (std::uint8_t v : list)
    {
        std::size_t idx = 0;
        while (table[idx] != v)
            ++idx;
        file.push_back(static_cast<std::uint8_t>(idx));
        table.erase(table.begin() + idx);
        table.insert(table.begin(), v);
    }
    file.push_back(0);

    // The header bytes are MTF-decoded too, so skip them the way the loader does.
    const Bytes out = assets::bwtDecompress(file);
    CHECK(std::string(out.begin(), out.end()) == "AAA");
    CHECK(assets::isBwtCompressed(file));
    CHECK(assets::bwtDecompress(Bytes{1, 2, 3}).empty());
}

TEST_CASE("utf conversion and word capitalization")
{
    CHECK(text::utf8ToUtf16("a\xC3\xA9\xE4\xB8\x80") == u"aé一");
    CHECK(text::utf16ToUtf8(u"aé一") == "a\xC3\xA9\xE4\xB8\x80");
    CHECK(text::utf8ToUtf16("\xF0\x9F\x98\x80") == u"\U0001F600");
    CHECK(text::utf8ToUtf16("\xFF") == u"�");
    CHECK(text::capitalizeAllWords(std::string_view("hello  big world")) == "Hello  Big World");
    CHECK(text::capitalizeAllWords(std::string_view("\xC3\xA9t\xC3\xA9")) == "\xC3\x89t\xC3\xA9");
}

TEST_CASE("speech hue and time to live follow MessageManager")
{
    CHECK(text::fixSpeechHue(0) == 0);
    CHECK(text::fixSpeechHue(0x8000) == 0x8000);
    CHECK(text::fixSpeechHue(0x0035) == 0x0035);
    CHECK(text::fixSpeechHue(0x0BB8) == 1);
    CHECK(text::fixSpeechHue(0xC0BB) == 0xC0BB);

    CHECK(text::speechTimeToLive(1, {true, 100}) == 4000);
    CHECK(text::speechTimeToLive(3, {true, 5}) == 1200);  // delay clamps to 10
    CHECK(text::speechTimeToLive(2, {false, 100}) == 4000);

    Fixture fx;
    const std::u16string shortText = u"hi";
    CHECK(text::speechLayoutWidth(fx.fonts, 0, true, shortText) == 0);
    std::u16string longText(80, u'x');
    CHECK(text::speechLayoutWidth(fx.fonts, 0, true, longText) > 0);
}

TEST_CASE("journal lines, affixes and fonts follow the original")
{
    Fixture fx;
    CHECK(text::journalLine("Brett", "hail") == "Brett: hail");
    CHECK(text::journalLine(" ", "You see: a sword") == "You see: a sword");
    CHECK(text::applyAffix("text", " !", false) == "text !");
    CHECK(text::applyAffix("text", "> ", true) == "> text");
    CHECK(text::applyAffix("text", "  ", true) == "text");

    CHECK(text::journalFont(false).font == 9);
    CHECK(text::journalFont(false, true).unicode);
    CHECK(text::speechFont(2, true, fx.fonts).font == 0);  // unifont2 is not in the fixture
    CHECK(text::speechFont(0, true, fx.fonts).font == 0);
    CHECK(text::speechFont(40, false, fx.fonts).font == 3);
    CHECK(text::speechFont(6, false, fx.fonts).font == 6);
}
