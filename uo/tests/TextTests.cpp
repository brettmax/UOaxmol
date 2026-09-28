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

#include <algorithm>
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


struct HtmlCase
{
    char kind;  // 'H' unicode, 'Q' ASCII height and text-by-width
    int font, flags, width, align, hue, cell, height;
    std::uint32_t startColor;
    bool background;
    const char* text;
};

const HtmlCase kHtmlCases[] = {
    {'H', 0, 0, 200, 0, 0xFFFF, 30, 0, 0xFFFFFFFF, false,
     "<basefont color=#FF0000>Red <b>bold</b> and <i>italic</i> <u>under</u></basefont> plain"},
    {'H', 0, 0, 160, 0, 0xFFFF, 30, 0, 0xFFFFFFFF, false,
     "<center>Centered title</center><p>First paragraph of text that wraps around.</p><right>right</right>"},
    {'H', 0, 0, 180, 0, 0xFFFF, 30, 0, 0xFFFFFFFF, false,
     "Visit <a href=\"http://a.com\">our site</a> or <a href=visited>this one</a> today"},
    {'H', 0, 0, 150, 0, 0xFFFF, 30, 0, 0xFFFFFFFF, true,
     "<body text=#00FF00 bgcolor=#101010 leftmargin=6 topmargin=4 rightmargin=3 bottommargin=2>Body with margins and "
     "background</body>"},
    {'H', 0, 0, 120, 1, 0x21, 30, 0, 0xFFFFFFFF, false,
     "<h1>Head</h1><br>line<br/>two<BR>three<bq>quoted text</bq><div align=right>div</div>"},
    {'H', 0, 0x40, 70, 0, 0xFFFF, 30, 0, 0x0000FFFF, false, "<b>Averyveryverylongsingleword</b> tail"},
    {'H', 0, 0, 0, 0, 0xFFFF, 30, 0, 0xFFFFFFFF, false,
     "<basefont color=lime size=7>No width <big>big</big> <small>small</small>"},
    {'H', 0, 0x200, 60, 0, 0xFFFF, 30, 40, 0xFFFFFFFF, false,
     "<i>crop texture keeps </i>going over several lines of html"},
    {'H', 0, 0, 100, 0, 0xFFFF, 30, 0, 0xFFFFFFFF, false, "a < b and <unknown>tag</unknown> <><b"},
    // Only tags and a crop-texture height: the original never returned here.
    {'H', 0, 0x200, 20, 0, 0xFFFF, 30, 30, 0xFFFFFFFF, false, "<basefont color=red><b><i><u></u></i></b>"},
    {'H', 1, 0x8, 140, 2, 0xFFFF, 30, 0, 0x010101FF, false,
     "<p align=center>Black <basefont color=white>white</basefont> text with a border</p>"},
    {'Q', 1, 0, 100, 0, 0, 30, 0, 0xFFFFFFFF, false, "<b>ascii</b> html text here"},
    {'Q', 3, 0, 40, 0, 0, 30, 0, 0xFFFFFFFF, false, "<i>tags</i> first and a longer tail"},
};

// The original's output for kHtmlCases, with "visited" marked as a visited link: per 'H'
// case h/x/c/t/l/g where g adds the <body> background and the links as
// url|x|y|width|height, and per 'Q' case h/c/t.
const char* const kHtmlExpected[] = {
    "h 36",
    "x 200",
    "c 003C00620061007300650066006F006E007400200063006F006C006F0072003D0023004600460030003000300030003E0052006500640020003C0062003E0062006F006C0064003C002F0062003E00200061006E00640020003C0069003E006900740061006C00690063003C002F00690020003C00750075006E006400650072003C0075003C0061007300650066006F006E007400200070002E002E002E",
    "t 003C00620061007300650066006F006E007400200063006F006C006F0072003D0023004600460030003000300030003E0052006500640020003C0062003E0062006F006C0064003C002F0062003E00200061006E00640020003C0069003E006900740061006C00690063003C002F00690020003C00750075006E006400650072003C0075003C0061007300650066006F006E00740020007000610069",
    "l #984D986E0E25083C",
    "g 204 40 2 E78E409C222A42E0 0 -",
    "h 126",
    "x 155",
    "c 003C00630065006E007400650072003E00430065006E007400650072006500640020007400690074006C0065003C002F00630065006E007400650072003E003C0070003E0046006900720073007400200070006100720061006700720061007000680020002E002E002E",
    "t 003C00630065006E007400650072003E00430065006E007400650072006500640020007400690074006C0065003C002F00630065006E007400650072003E003C0070003E0046006900720073007400200070006100720061006700720061007000680020006F00660020",
    "l #523B5A797536447C",
    "g 164 130 7 DA3B6552EC09C27B 0 -",
    "h 36",
    "x 182",
    "c 005600690073006900740020003C006100200068007200650066003D00220068007400740070003A002F002F0061002E0063006F006D0022003E006F0075007200200073006900740065003C002F0061003E0020006F00720020003C006100200068007200650066003D007600690073006900740065006400740068006900730020006F002E002E002E",
    "t 005600690073006900740020003C006100200068007200650066003D00220068007400740070003A002F002F0061002E0063006F006D0022003E006F0075007200200073006900740065003C002F0061003E0020006F00720020003C006100200068007200650066003D007600690073006900740065006400740068006900730020006F006E0065003C",
    "l #9F7F87289507BE0A",
    "g 184 40 2 84CC8DD81056340E 0 0068007400740070003A002F002F0061002E0063006F006D|47|3|110|14;0076006900730069007400650064|155|3|171|14;0076006900730069007400650064|0|21|27|14",
    "h 36",
    "x 130",
    "c 003C0062006F0064007900200074006500780074003D00230030003000460046003000300020006200670063006F006C006F0072003D00230031003000310030003100300020006C006500660074006D0061007200670069006E003D003600200074006F0070006D0061007200670069006E003D0034002000720069006700680074006D0061007200670069006E003D003300200062006F00740074006F006D006D0061007200670069006E003D0032003E0042006F00640079002000770069007400680020006D0061007200670069006E007300200061006E00640020002E002E002E",
    "t 003C0062006F0064007900200074006500780074003D00230030003000460046003000300020006200670063006F006C006F0072003D00230031003000310030003100300020006C006500660074006D0061007200670069006E003D003600200074006F0070006D0061007200670069006E003D0034002000720069006700680074006D0061007200670069006E003D003300200062006F00740074006F006D006D0061007200670069006E003D0032003E0042006F00640079002000770069007400680020006D0061007200670069006E007300200061006E0064002000610063006B",
    "l #CBAE8768C121DFB2",
    "g 154 46 2 8ECF690C2E9024C3 FF101010 -",
    "h 108",
    "x 89",
    "c 003C00680031003E0048006500610064003C002F00680031003E003C00620072003E006C0069006E0065003C00620072002F003E00740077006F003C00420052003E00740068007200650065003C00620071003E00710075006F00740065006400200074006500780074003C003C00640069007600200061002E002E002E",
    "t 003C00680031003E0048006500610064003C002F00680031003E003C00620072003E006C0069006E0065003C00620072002F003E00740077006F003C00420052003E00740068007200650065003C00620071003E00710075006F00740065006400200074006500780074003C003C0064006900760020006100690067006E",
    "l #6BF79881F197C62D",
    "g 124 112 6 362F9BEFF1E0B7D3 0 -",
    "h 18",
    "x 132",
    "c 003C0062003E004100760065007200790076006500720079002E002E002E",
    "t 003C0062003E0041007600650072007900760065007200790076006500720079",
    "l #1277AC01D68F0741",
    "g 74 22 1 3D4C05ED5401CFF7 0 -",
    "h 18",
    "x 17",
    "c 003C00620061007300650066006F006E007400200063006F006C006F0072003D006C0069006D0065002000730069007A0065003D0037003E004E006F0020007700690064007400680020003C006200690067003E006200690067003C002F006200690067003E0020003C0073002E002E002E",
    "t 003C00620061007300650066006F006E007400200063006F006C006F0072003D006C0069006D0065002000730069007A0065003D0037003E004E006F0020007700690064007400680020003C006200690067003E006200690067003C002F006200690067003E0020003C0073",
    "l #79DD9CC46B80234C",
    "g 397 22 1 4D47B8A8A9E5FB3B 0 -",
    "h 162",
    "x 62",
    "c 003C0069003E00630072006F0070002000740065002E002E002E",
    "t 003C0069003E00630072006F00700020007400650078007400750072",
    "l #55DF330649C82E26",
    "g 64 76 4 3174610403ADC1AC 0 -",
    "h 18",
    "x 60",
    "c 00610020003C0020006200200061006E00640020003C0075006E006B006E006F0077006E003E007400610067003C002F0075006E006B006E006F0077006E0020003C003C",
    "t 00610020003C0020006200200061006E00640020003C0075006E006B006E006F0077006E003E007400610067003C002F0075006E006B006E006F0077006E0020003C003C",
    "l #2519AD69CBD8579B",
    "g 104 22 1 5EDA9DA469FA1653 0 -",
    "h 0",
    "x 4",
    "c 003C00620061007300650066006F006E007400200063006F006C006F0072003D007200650064003E003C0062003E003C0069003E003C0075003E003C002F0075003E003C002F0069003E003C002F0062003E",
    "t 003C00620061007300650066006F006E007400200063006F006C006F0072003D007200650064003E003C0062003E003C0069003E003C0075003E003C002F0075003E003C002F0069003E003C002F0062003E",
    "l #14650FB0739D0383",
    "g 0 0 0 0 0 -",
    "h 54",
    "x 113",
    "c 003C007000200061006C00690067006E003D00630065006E007400650072003E0042006C00610063006B0020003C00620061007300650066006F006E007400200063006F006C006F0072003D00770068006900740065003E00770068006900740065003C002F00620061007300650066006F006E0074002000740065007800740020002E002E002E",
    "t 003C007000200061006C00690067006E003D00630065006E007400650072003E0042006C00610063006B0020003C00620061007300650066006F006E007400200063006F006C006F0072003D00770068006900740065003E00770068006900740065003C002F00620061007300650066006F006E00740020007400650078007400200077006900740068",
    "l #F13F4EA01CB6B590",
    "g 144 58 3 7046DD03B20D757D 0 -",
    "h 36",
    "c 003C0062003E00610073006300690069003C002F0062003E002000680074006D006C0020007400650078007400200068006500720065",
    "t 003C0062003E00610073006300690069003C002F0062003E002000680074006D006C0020007400650078007400200068006500720065",
    "h 108",
    "c 003C0069003E0074006100670073003C002F002E002E002E",
    "t 003C0069003E0074006100670073003C002F0069003E002000660069",
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

TEST_CASE("HTML text matches the original client on the fixture")
{
    Fixture fx;
    auto& r = fx.fonts;
    r.markUrlVisited("visited");
    std::vector<std::string> actual;

    for (const auto& c : kHtmlCases)
    {
        const std::u16string s = text::utf8ToUtf16(c.text);
        const auto f           = static_cast<std::uint8_t>(c.font);
        const auto fl          = static_cast<std::uint16_t>(c.flags);
        const auto al          = static_cast<text::TextAlign>(c.align);
        text::FontRenderer::HtmlScope html(r, c.startColor, c.background);

        if (c.kind == 'Q')
        {
            actual.push_back("h " + std::to_string(r.heightAscii(f, s, c.width, al, fl)));
            actual.push_back("c " + hex16(r.textByWidthAscii(f, s, c.width, true)));
            actual.push_back("t " + hex16(r.textByWidthAscii(f, s, c.width, false)));
            continue;
        }

        actual.push_back("h " + std::to_string(r.heightUnicode(f, s, c.width, al, fl)));
        actual.push_back("x " + std::to_string(r.widthExUnicode(f, s, c.width, al, fl)));
        actual.push_back("c " + hex16(r.textByWidthUnicode(f, s, c.width, true)));
        actual.push_back("t " + hex16(r.textByWidthUnicode(f, s, c.width, false)));
        actual.push_back("l #" + hexPadded(fnvBytes(layoutString(r.layoutUnicode(f, s, al, fl, c.width)))));

        auto g = r.generateUnicode(f, s, static_cast<std::uint16_t>(c.hue), static_cast<std::uint8_t>(c.cell), c.width,
                                   al, fl, c.height);
        std::string line = "g " + std::to_string(g.width) + " " + std::to_string(g.height) + " " +
                           std::to_string(g.lineCount) + " " + hexU(g.pixels.empty() ? 0 : fnvPixels(g.pixels)) + " " +
                           hexU(g.htmlBackgroundColor) + " ";
        if (g.links.empty())
            line += "-";
        for (std::size_t i = 0; i < g.links.size(); i++)
        {
            const auto& l = g.links[i];
            line += (i ? ";" : "") + hex16(text::utf8ToUtf16(l.url)) + "|" + std::to_string(l.x) + "|" +
                    std::to_string(l.y) + "|" + std::to_string(l.width) + "|" + std::to_string(l.height);
        }
        actual.push_back(line);
    }

    REQUIRE(actual.size() == std::size(kHtmlExpected));

    for (std::size_t i = 0; i < actual.size(); ++i)
    {
        CAPTURE(i);
        CHECK(actual[i] == kHtmlExpected[i]);
    }
}

TEST_CASE("HTML mode is scoped, and links and backgrounds come back with the bitmap")
{
    Fixture fx;
    auto& r = fx.fonts;
    const std::u16string tagged = u"<b>bold</b>";

    // Outside HTML mode tags are plain characters.
    const int plainWidth = r.widthExUnicode(0, tagged, 400, text::TextAlign::Left, 0);
    {
        text::FontRenderer::HtmlScope html(r);
        CHECK(r.usingHtml());
        CHECK(r.widthExUnicode(0, tagged, 400, text::TextAlign::Left, 0) < plainWidth);
        CHECK(r.heightUnicode(0, tagged, 400, text::TextAlign::Left, 0) == 18);

        auto g = r.generateUnicode(0, u"go <a href=\"http://x\">here</a> now", 0xFFFF, 30, 300, text::TextAlign::Left, 0);
        REQUIRE(g.links.size() == 1);
        CHECK(g.links[0].url == "http://x");
        CHECK(g.links[0].contains(g.links[0].x, g.links[0].y));
        CHECK_FALSE(g.links[0].contains(g.links[0].x + g.links[0].width, g.links[0].y));

        CHECK_FALSE(r.urlVisited("http://x"));
        r.markUrlVisited("http://x");
        CHECK(r.urlVisited("http://x"));
    }
    CHECK_FALSE(r.usingHtml());
    CHECK(r.widthExUnicode(0, tagged, 400, text::TextAlign::Left, 0) == plainWidth);

    // <body bgcolor> only paints when the caller allows it.
    const std::u16string body = u"<body bgcolor=#204060>x</body>";
    {
        text::FontRenderer::HtmlScope html(r, 0xFFFFFFFF, false);
        CHECK(r.generateUnicode(0, body, 0xFFFF, 30, 50, text::TextAlign::Left, 0).htmlBackgroundColor == 0);
    }
    {
        text::FontRenderer::HtmlScope html(r, 0xFFFFFFFF, true);
        auto g = r.generateUnicode(0, body, 0xFFFF, 30, 50, text::TextAlign::Left, 0);
        CHECK(g.htmlBackgroundColor != 0);
        CHECK(std::find(g.pixels.begin(), g.pixels.end(), 0u) == g.pixels.end());
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
