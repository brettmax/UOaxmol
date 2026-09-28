// SPDX-License-Identifier: BSD-2-Clause
// Part of the AxmolUO client core. Ported from ClassicUO (PacketHandlers.CreateGump,
// Utility/TextFileParser and the gump controls' layout constructors).
//
// Server gump layout language (packets 0xB0 and 0xDD).
//
// A server gump arrives as a layout string such as
//
//     { page 0 }{ resizepic 0 0 9200 300 200 }{ button 20 160 247 248 1 0 1 }
//     { text 20 20 0 0 }{ tooltip 1011036 }
//
// plus a table of Unicode text lines that `text`, `croppedtext`, `htmlgump`
// and the text entries refer to by index. This file turns that pair into a
// flat list of typed elements; it has no engine dependency so it can be
// unit tested and reused by tools. The Axmol side (uo/client/gumps) builds
// nodes from the result.

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace uo::gumps
{

// Splits a layout string into its `{ ... }` commands, then a command into its
// parameters. Parameters are separated by spaces or commas; `@...@` quotes a
// parameter that may contain either (used by `xmfhtmltok` and `tooltip`
// argument lists).
std::vector<std::string_view> splitCommands(std::string_view layout);
std::vector<std::string_view> splitParams(std::string_view command);

// ClassicUO's UInt16Converter.Parse: decimal, "0x" hex, or a negative value
// that wraps (servers send -1 or 4294967295 for "no hue"). Never fails; bad
// input reads as 0 like the C# original.
uint16_t parseU16(std::string_view s);

// ClassicUO's SerialHelper.Parse (decimal, "0x" hex, or negative int).
std::optional<uint32_t> parseSerial(std::string_view s);

// Strict signed integer; std::nullopt on anything that is not a number.
std::optional<int32_t> parseInt(std::string_view s);

enum class ElementType : uint8_t
{
    Button,          // button x y normal pressed type page id
    ButtonTileArt,   // buttontileart x y normal pressed type page id tile hue w h
    CheckerTrans,    // checkertrans x y w h
    CroppedText,     // croppedtext x y w h hue textIndex
    GumpPic,         // gumppic x y id [hue=H] [class=VirtueGumpItem]; also tilepicasgumppic, gumppichued, gumppicphued
    GumpPicTiled,    // gumppictiled x y w h id
    PicInPic,        // picinpic x y id sx sy w h [hue]; also picinpichued, picinpicphued
    Html,            // htmlgump x y w h textIndex background scrollbar
    XmfHtml,         // xmfhtmlgump / xmfhtmlgumpcolor / xmfhtmltok (text is a cliloc)
    ResizePic,       // resizepic x y id w h
    Text,            // text x y hue textIndex
    TextEntry,       // textentry x y w h hue id textIndex; textentrylimited adds maxLength
    TilePic,         // tilepic x y id; tilepichue x y id hue
    Checkbox,        // checkbox x y off on checked id
    Radio,           // radio x y off on checked id (in the current group)
};

// Scrollbar style of an HTML element: none, the classic bar, or the small
// "flag" that drags along the edge (value 2 on the wire).
enum class ScrollStyle : uint8_t
{
    None,
    Bar,
    Flag,
};

// One cliloc tooltip line attached to the element before it.
struct TooltipLine
{
    uint32_t cliloc = 0;
    // Tab separated arguments as the cliloc formatter expects them, or empty.
    std::string args;
};

struct GumpElement
{
    ElementType type{};
    int page = 0;   // 0 draws on every page
    int group = 0;  // radio button group

    int x = 0, y = 0;
    int width = 0, height = 0;  // 0 when the size comes from the art

    uint16_t graphic = 0;         // gump or art id (normal state for buttons/checkboxes)
    uint16_t graphicPressed = 0;  // button pressed / checkbox checked
    uint16_t hue = 0;
    bool partialHue = false;

    // Buttons: activate (reply to server with `id`) or switch to `toPage`.
    bool activates = false;
    int toPage = 0;
    // Button id, checkbox/radio switch id, or text entry id.
    uint32_t id = 0;
    bool checked = false;

    // picinpic source rectangle; buttontileart tile art, hue and cell size.
    int srcX = 0, srcY = 0;
    uint16_t tileGraphic = 0;
    uint16_t tileHue = 0;

    // Resolved text (UTF-8) for text, croppedtext, htmlgump and text entries.
    std::string text;
    // xmfhtml* elements: the cliloc to resolve and its tab separated args.
    uint32_t cliloc = 0;
    std::string clilocArgs;
    uint32_t htmlColor = 0;  // RGB for xmfhtmlgumpcolor / xmfhtmltok, 0 = default
    bool background = false;
    ScrollStyle scroll = ScrollStyle::None;
    int maxLength = 0;  // textentrylimited, 0 = unlimited (ClassicUO uses 255)

    bool virtue = false;           // gumppic with class=VirtueGumpItem
    std::vector<TooltipLine> tooltips;
    std::optional<uint32_t> itemProperty;  // itemproperty serial for an OPL tooltip
};

struct GumpLayout
{
    uint32_t sender = 0;  // "local" serial in ClassicUO; echoed in the reply
    uint32_t gumpId = 0;  // "server" serial; echoed in the reply
    int x = 0, y = 0;

    bool canMove = true;
    bool canCloseWithRightClick = true;  // `noclose` clears it
    bool canCloseWithEsc = true;         // `nodispose` clears it
    uint32_t masterGump = 0;

    std::vector<GumpElement> elements;
    // Commands the parser did not recognise, for logging.
    std::vector<std::string> unknownCommands;

    // Highest page number referenced; pages run 1..pageCount.
    int pageCount() const;
};

// Parses a layout string plus its text lines (already decoded to UTF-8).
// Malformed commands are skipped, never fatal: a server gump with one bad
// element still opens, which is what players expect from the C# client.
GumpLayout parseLayout(std::string_view layout, const std::vector<std::string>& lines);

// Hue for a `hue=` parameter or a text/entry hue on the wire. The wire value is
// zero-based for text (ClassicUO adds 1), and gumppic hues 0..2 mean "none".
uint16_t gumpPicHue(uint16_t wireHue);
uint16_t textHue(uint16_t wireHue);

}  // namespace uo::gumps
