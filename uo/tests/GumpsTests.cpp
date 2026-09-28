// SPDX-License-Identifier: BSD-2-Clause
// Tests for uo::gumps: the server gump layout language and 0xB0/0xDD/0xB1.

#include "doctest.h"

#include "uo/gumps/GumpLayout.h"
#include "uo/gumps/GumpPackets.h"
#include "uo/io/BinaryReader.h"

#include "uo/io/Compression.h"

#include <string>
#include <vector>

using namespace uo::gumps;

namespace
{

void put16(std::vector<uint8_t>& b, uint16_t v)
{
    b.push_back(static_cast<uint8_t>(v >> 8));
    b.push_back(static_cast<uint8_t>(v));
}

void put32(std::vector<uint8_t>& b, uint32_t v)
{
    put16(b, static_cast<uint16_t>(v >> 16));
    put16(b, static_cast<uint16_t>(v));
}

void putUnicodeLine(std::vector<uint8_t>& b, std::u16string_view s)
{
    put16(b, static_cast<uint16_t>(s.size()));

    for (char16_t c : s)
    {
        put16(b, static_cast<uint16_t>(c));
    }
}

std::vector<uint8_t> deflate(const std::vector<uint8_t>& src)
{
    return uo::io::deflate(src);
}

const GumpElement& only(const GumpLayout& g)
{
    REQUIRE(g.elements.size() == 1);
    return g.elements[0];
}

}  // namespace

TEST_CASE("gumps: splitCommands takes the contents of each brace pair")
{
    auto cmds = splitCommands("{ page 0 }{resizepic 0 0 9200 300 200}  {  nomove  }\n{ text 1 2 3 4 }");
    REQUIRE(cmds.size() == 4);
    CHECK(cmds[0] == "page 0");
    CHECK(cmds[1] == "resizepic 0 0 9200 300 200");
    CHECK(cmds[2] == "nomove");
    CHECK(cmds[3] == "text 1 2 3 4");
}

TEST_CASE("gumps: splitParams handles commas and @-quoted arguments")
{
    auto p = splitParams("xmfhtmltok 10,20 100 40 0 0 32767 1049644 @Hello\tWorld@");
    REQUIRE(p.size() == 10);
    CHECK(p[1] == "10");
    CHECK(p[2] == "20");
    CHECK(p[8] == "1049644");
    CHECK(p[9] == "Hello\tWorld");

    auto t = splitParams("tooltip 1060658 @Weight@");
    REQUIRE(t.size() == 3);
    CHECK(t[2] == "Weight");
}

TEST_CASE("gumps: number parsing matches ClassicUO's converters")
{
    CHECK(parseU16("0x1F") == 0x1F);
    CHECK(parseU16("-1") == 0xFFFF);
    CHECK(parseU16("4294967295") == 0xFFFF);
    CHECK(parseU16("junk") == 0);
    CHECK(parseSerial("0x40000001") == 0x40000001u);
    CHECK(parseSerial("-2") == 0xFFFFFFFEu);
    CHECK_FALSE(parseInt("12a").has_value());
    CHECK(gumpPicHue(0) == 0);
    CHECK(gumpPicHue(1) == 0);
    CHECK(gumpPicHue(2) == 3);
    CHECK(textHue(0) == 1);
}

TEST_CASE("gumps: buttons, pages and flags")
{
    auto g = parseLayout("{nomove}{noclose}{nodispose}{page 0}{resizepic 0 0 9200 300 200}"
                         "{page 1}{button 10 20 247 248 1 0 5}{button 10 40 4005 4007 0 2 0}"
                         "{page 2}{text 5 5 32 0}",
                         {"Hi"});

    CHECK_FALSE(g.canMove);
    CHECK_FALSE(g.canCloseWithRightClick);
    CHECK_FALSE(g.canCloseWithEsc);
    REQUIRE(g.elements.size() == 4);

    CHECK(g.elements[0].type == ElementType::ResizePic);
    CHECK(g.elements[0].page == 0);
    CHECK(g.elements[0].graphic == 9200);
    CHECK(g.elements[0].width == 300);

    const auto& reply = g.elements[1];
    CHECK(reply.type == ElementType::Button);
    CHECK(reply.page == 1);
    CHECK(reply.activates);
    CHECK(reply.id == 5);
    CHECK(reply.graphic == 247);
    CHECK(reply.graphicPressed == 248);

    const auto& pager = g.elements[2];
    CHECK_FALSE(pager.activates);
    CHECK(pager.toPage == 2);

    CHECK(g.elements[3].type == ElementType::Text);
    CHECK(g.elements[3].page == 2);
    CHECK(g.elements[3].hue == 33);
    CHECK(g.elements[3].text == "Hi");
    CHECK(g.pageCount() == 2);
}

TEST_CASE("gumps: text lines out of range read as empty")
{
    auto g = parseLayout("{text 0 0 0 7}", {"a"});
    CHECK(only(g).text.empty());
}

TEST_CASE("gumps: gumppic hue and virtue class")
{
    auto g = parseLayout("{gumppic 1 2 100 hue=33}{gumppic 0 0 0x6F hue=2405 class=VirtueGumpItem}"
                         "{gumppicphued 0 0 50 hue=10}",
                         {});
    REQUIRE(g.elements.size() == 3);
    CHECK(g.elements[0].hue == 34);
    CHECK_FALSE(g.elements[0].virtue);
    CHECK(g.elements[1].graphic == 0x6F);
    CHECK(g.elements[1].virtue);
    CHECK(g.elements[2].partialHue);
}

TEST_CASE("gumps: radio groups and checkboxes")
{
    auto g = parseLayout("{group 0}{radio 0 0 208 209 1 100}{radio 0 20 208 209 0 101}{endgroup}"
                         "{group 1}{radio 0 40 208 209 0 200}{checkbox 0 60 210 211 1 300}",
                         {});
    REQUIRE(g.elements.size() == 4);
    CHECK(g.elements[0].group == g.elements[1].group);
    CHECK(g.elements[2].group != g.elements[0].group);
    CHECK(g.elements[0].checked);
    CHECK(g.elements[0].id == 100);
    CHECK(g.elements[3].type == ElementType::Checkbox);
    CHECK(g.elements[3].id == 300);
}

TEST_CASE("gumps: text entries")
{
    auto g = parseLayout("{textentry 10 10 200 20 0 7 0}{textentrylimited 10 40 200 20 5 8 1 12}", {"Name", "Pw"});
    REQUIRE(g.elements.size() == 2);
    CHECK(g.elements[0].id == 7);
    CHECK(g.elements[0].text == "Name");
    CHECK(g.elements[0].maxLength == 255);
    CHECK(g.elements[1].maxLength == 12);
    CHECK(g.elements[1].hue == 6);
}

TEST_CASE("gumps: html and cliloc html")
{
    auto g = parseLayout("{htmlgump 0 0 100 50 0 1 2}"
                         "{xmfhtmlgump 0 0 100 50 1011036 1 0}"
                         "{xmfhtmlgumpcolor 0 0 100 50 #1011037 0 1 32767}"
                         "{xmfhtmltok 0 0 100 50 1 2 16 1049644 @one@two@}",
                         {"<b>hi</b>"});
    REQUIRE(g.elements.size() == 4);

    CHECK(g.elements[0].type == ElementType::Html);
    CHECK(g.elements[0].text == "<b>hi</b>");
    CHECK(g.elements[0].background);
    CHECK(g.elements[0].scroll == ScrollStyle::Flag);

    CHECK(g.elements[1].cliloc == 1011036);
    CHECK(g.elements[1].background);
    CHECK(g.elements[1].scroll == ScrollStyle::None);

    CHECK(g.elements[2].cliloc == 1011037);
    CHECK(g.elements[2].htmlColor == 0x00FFFFFF);
    CHECK(g.elements[2].scroll == ScrollStyle::Bar);

    CHECK(g.elements[3].cliloc == 1049644);
    CHECK(g.elements[3].htmlColor == 16);
    CHECK(g.elements[3].scroll == ScrollStyle::Flag);
    CHECK(g.elements[3].clilocArgs == "one\ttwo");
}

TEST_CASE("gumps: tooltip and itemproperty attach to the previous element")
{
    auto g = parseLayout("{tooltip 1000}{gumppic 0 0 1}{tooltip 1060658 Weight 5}{tooltip 1001}"
                         "{itemproperty 0x40000010}",
                         {});
    const auto& e = only(g);
    REQUIRE(e.tooltips.size() == 2);
    CHECK(e.tooltips[0].cliloc == 1060658);
    CHECK(e.tooltips[0].args == "Weight\t5");
    CHECK(e.tooltips[1].args.empty());
    CHECK(e.itemProperty == 0x40000010u);
}

TEST_CASE("gumps: buttontileart, picinpic, tilepic, checkertrans, gumppictiled")
{
    auto g = parseLayout("{buttontileart 10 10 2151 2153 1 0 9 0x1F03 33 40 50}"
                         "{picinpicphued 0 0 500 5 6 20 30 44}"
                         "{tilepichue 1 1 3821 1161}{checkertrans 0 0 50 50}{gumppictiled 0 0 80 90 2624}",
                         {});
    REQUIRE(g.elements.size() == 5);
    CHECK(g.elements[0].tileGraphic == 0x1F03);
    CHECK(g.elements[0].tileHue == 33);
    CHECK(g.elements[0].width == 40);
    CHECK(g.elements[0].height == 50);
    CHECK(g.elements[0].id == 9);
    CHECK(g.elements[1].srcX == 5);
    CHECK(g.elements[1].height == 30);
    CHECK(g.elements[1].hue == 44);
    CHECK(g.elements[1].partialHue);
    CHECK(g.elements[2].hue == 1161);
    CHECK(g.elements[3].type == ElementType::CheckerTrans);
    CHECK(g.elements[4].graphic == 2624);
}

TEST_CASE("gumps: malformed and unknown commands are skipped")
{
    auto g = parseLayout("{button 1 2}{resizepic a b c d e}{sparkles 1 2}{mastergump 77}{text 0 0 0 0}", {"ok"});
    CHECK(g.masterGump == 77);
    REQUIRE(g.unknownCommands.size() == 1);
    CHECK(g.unknownCommands[0] == "sparkles");
    CHECK(only(g).type == ElementType::Text);
}

TEST_CASE("gumps: 0xB0 decode")
{
    std::string layout = "{page 0}{text 0 0 0 0}";
    std::vector<uint8_t> b{0xB0, 0, 0};
    put32(b, 0x1234);
    put32(b, 0xABCD);
    put32(b, 50);
    put32(b, 60);
    put16(b, static_cast<uint16_t>(layout.size() + 1));
    b.insert(b.end(), layout.begin(), layout.end());
    b.push_back(0);
    put16(b, 1);
    putUnicodeLine(b, u"Café");
    b[1] = static_cast<uint8_t>(b.size() >> 8);
    b[2] = static_cast<uint8_t>(b.size());

    auto g = decodeGump(b);
    REQUIRE(g.has_value());
    CHECK(g->sender == 0x1234);
    CHECK(g->gumpId == 0xABCD);
    CHECK(g->x == 50);
    CHECK(g->y == 60);
    CHECK(only(*g).text == "Caf\xC3\xA9");

    b.resize(b.size() - 3);
    CHECK_FALSE(decodeGump(b).has_value());
}

TEST_CASE("gumps: 0xDD decode")
{
    std::string layout = "{page 0}{croppedtext 0 0 100 20 0 1}";
    std::vector<uint8_t> text;
    putUnicodeLine(text, u"zero");
    putUnicodeLine(text, u"one");

    auto clayout = deflate(std::vector<uint8_t>(layout.begin(), layout.end()));
    auto ctext = deflate(text);

    std::vector<uint8_t> b{0xDD, 0, 0};
    put32(b, 1);
    put32(b, 2);
    put32(b, 3);
    put32(b, 4);
    put32(b, static_cast<uint32_t>(clayout.size() + 4));
    put32(b, static_cast<uint32_t>(layout.size()));
    b.insert(b.end(), clayout.begin(), clayout.end());
    put32(b, 2);
    put32(b, static_cast<uint32_t>(ctext.size() + 4));
    put32(b, static_cast<uint32_t>(text.size()));
    b.insert(b.end(), ctext.begin(), ctext.end());

    auto g = decodeGump(b);
    REQUIRE(g.has_value());
    CHECK(g->gumpId == 2);
    CHECK(only(*g).text == "one");

    b[40] ^= 0xFF;  // corrupt the compressed layout
    CHECK_FALSE(decodeGump(b).has_value());
}

TEST_CASE("gumps: 0xB1 encode")
{
    std::vector<uint32_t> switches{100, 300};
    std::vector<std::pair<uint16_t, std::string>> entries{{7, "Hi\xC3\xA9"}};
    auto p = encodeGumpResponse(0x11, 0x22, 5, switches, entries);

    std::vector<uint8_t> want{0xB1, 0, 0};
    put32(want, 0x11);
    put32(want, 0x22);
    put32(want, 5);
    put32(want, 2);
    put32(want, 100);
    put32(want, 300);
    put32(want, 1);
    put16(want, 7);
    putUnicodeLine(want, u"Hié");
    want[2] = static_cast<uint8_t>(want.size());
    CHECK(p == want);

    std::vector<std::pair<uint16_t, std::string>> longEntry{{1, std::string(300, 'x')}};
    auto q = encodeGumpResponse(0, 0, 0, {}, longEntry);
    CHECK(q.size() == 3 + 4 * 5 + 4 + 239 * 2);
}

TEST_CASE("gumps: UTF-16 round trip with surrogates")
{
    std::string s = "a\xF0\x9F\x98\x80z";
    std::u16string u = utf8ToUtf16(s);
    REQUIRE(u.size() == 4);

    std::vector<uint8_t> be;

    for (char16_t c : u)
    {
        put16(be, static_cast<uint16_t>(c));
    }

    uo::io::BinaryReader r(be);
    CHECK(r.readUnicodeBE(u.size()) == s);
}
