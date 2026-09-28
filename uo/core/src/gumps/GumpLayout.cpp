// SPDX-License-Identifier: BSD-2-Clause
#include "uo/gumps/GumpLayout.h"

#include <algorithm>
#include <charconv>

namespace uo::gumps
{

namespace
{

bool iequals(std::string_view a, std::string_view b)
{
    if (a.size() != b.size())
    {
        return false;
    }

    for (size_t i = 0; i < a.size(); ++i)
    {
        char ca = a[i], cb = b[i];

        if (ca >= 'A' && ca <= 'Z')
        {
            ca = static_cast<char>(ca - 'A' + 'a');
        }

        if (cb >= 'A' && cb <= 'Z')
        {
            cb = static_cast<char>(cb - 'A' + 'a');
        }

        if (ca != cb)
        {
            return false;
        }
    }

    return true;
}

bool icontains(std::string_view haystack, std::string_view needle)
{
    if (needle.size() > haystack.size())
    {
        return false;
    }

    for (size_t i = 0; i + needle.size() <= haystack.size(); ++i)
    {
        if (iequals(haystack.substr(i, needle.size()), needle))
        {
            return true;
        }
    }

    return false;
}

bool isParamDelimiter(char c)
{
    return c == ' ' || c == ',';
}

// Text line lookup that tolerates bad indices the way ClassicUO's Label does.
std::string lineAt(const std::vector<std::string>& lines, std::string_view index)
{
    auto i = parseInt(index);

    if (!i || *i < 0 || static_cast<size_t>(*i) >= lines.size())
    {
        return {};
    }

    return lines[static_cast<size_t>(*i)];
}

// Joins params[from..] with tabs, the cliloc argument separator.
std::string joinArgs(const std::vector<std::string_view>& p, size_t from)
{
    std::string out;

    for (size_t i = from; i < p.size(); ++i)
    {
        if (i != from)
        {
            out += '\t';
        }

        out.append(p[i]);
    }

    return out;
}

// Cliloc numbers are sometimes sent as "#1049644".
std::optional<uint32_t> parseCliloc(std::string_view s)
{
    if (!s.empty() && s.front() == '#')
    {
        s.remove_prefix(1);
    }

    auto v = parseInt(s);

    if (!v || *v < 0)
    {
        return std::nullopt;
    }

    return static_cast<uint32_t>(*v);
}

// Required integer parameters; the element is skipped if any is missing or bad.
struct Ints
{
    const std::vector<std::string_view>& p;
    bool ok = true;

    int operator[](size_t i)
    {
        if (i >= p.size())
        {
            ok = false;
            return 0;
        }

        auto v = parseInt(p[i]);

        if (!v)
        {
            ok = false;
            return 0;
        }

        return *v;
    }
};

}  // namespace

std::vector<std::string_view> splitCommands(std::string_view layout)
{
    std::vector<std::string_view> out;
    size_t pos = 0;

    while (pos < layout.size())
    {
        size_t open = layout.find('{', pos);

        if (open == std::string_view::npos)
        {
            break;
        }

        size_t close = layout.find('}', open + 1);

        if (close == std::string_view::npos)
        {
            close = layout.size();
        }

        std::string_view cmd = layout.substr(open + 1, close - open - 1);

        // Some servers null-terminate the layout inside the last command.
        if (auto nul = cmd.find('\0'); nul != std::string_view::npos)
        {
            cmd = cmd.substr(0, nul);
        }

        size_t first = cmd.find_first_not_of(" \t\r\n");

        if (first != std::string_view::npos)
        {
            size_t last = cmd.find_last_not_of(" \t\r\n");
            out.push_back(cmd.substr(first, last - first + 1));
        }

        pos = close + 1;
    }

    return out;
}

std::vector<std::string_view> splitParams(std::string_view command)
{
    std::vector<std::string_view> out;
    size_t pos = 0;
    const size_t n = command.size();

    while (pos < n)
    {
        while (pos < n && isParamDelimiter(command[pos]))
        {
            ++pos;
        }

        if (pos >= n)
        {
            break;
        }

        if (command[pos] == '@')
        {
            size_t start = ++pos;
            size_t end = command.find('@', start);

            if (end == std::string_view::npos)
            {
                end = n;
            }

            if (end > start)
            {
                out.push_back(command.substr(start, end - start));
            }

            pos = end + 1;
        }
        else
        {
            size_t start = pos;

            while (pos < n && !isParamDelimiter(command[pos]) && command[pos] != '@')
            {
                ++pos;
            }

            out.push_back(command.substr(start, pos - start));
        }
    }

    return out;
}

std::optional<int32_t> parseInt(std::string_view s)
{
    if (s.empty())
    {
        return std::nullopt;
    }

    if (s.size() > 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X'))
    {
        uint32_t v = 0;
        auto [ptr, ec] = std::from_chars(s.data() + 2, s.data() + s.size(), v, 16);

        if (ec != std::errc{} || ptr != s.data() + s.size())
        {
            return std::nullopt;
        }

        return static_cast<int32_t>(v);
    }

    if (s.front() == '+')
    {
        s.remove_prefix(1);
    }

    int64_t v = 0;
    auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), v, 10);

    if (ec != std::errc{} || ptr != s.data() + s.size())
    {
        return std::nullopt;
    }

    // Accept the unsigned range too (servers write 0xFFFFFFFF as 4294967295).
    if (v < INT32_MIN || v > static_cast<int64_t>(UINT32_MAX))
    {
        return std::nullopt;
    }

    return static_cast<int32_t>(static_cast<uint32_t>(v));
}

uint16_t parseU16(std::string_view s)
{
    auto v = parseInt(s);
    return v ? static_cast<uint16_t>(*v) : 0;
}

std::optional<uint32_t> parseSerial(std::string_view s)
{
    auto v = parseInt(s);

    if (!v)
    {
        return std::nullopt;
    }

    return static_cast<uint32_t>(*v);
}

uint16_t gumpPicHue(uint16_t wireHue)
{
    uint16_t hue = static_cast<uint16_t>(wireHue + 1);
    return hue <= 2 ? 0 : hue;
}

uint16_t textHue(uint16_t wireHue)
{
    return static_cast<uint16_t>(wireHue + 1);
}

int GumpLayout::pageCount() const
{
    int count = 0;

    for (const auto& e : elements)
    {
        count = std::max({count, e.page, e.toPage});
    }

    return count;
}

GumpLayout parseLayout(std::string_view layout, const std::vector<std::string>& lines)
{
    GumpLayout g;

    int page = 0;
    int group = 0;

    for (std::string_view command : splitCommands(layout))
    {
        std::vector<std::string_view> p = splitParams(command);

        if (p.empty())
        {
            continue;
        }

        const std::string_view entry = p[0];
        Ints n{p};
        GumpElement e;
        e.page = page;
        bool add = false;

        if (iequals(entry, "button"))
        {
            e.type = ElementType::Button;
            e.x = n[1];
            e.y = n[2];
            e.graphic = parseU16(p.size() > 3 ? p[3] : "");
            e.graphicPressed = parseU16(p.size() > 4 ? p[4] : "");
            e.activates = p.size() >= 6 && n[5] != 0;
            e.toPage = p.size() >= 7 ? n[6] : 0;
            e.id = p.size() >= 8 ? static_cast<uint32_t>(n[7]) : 0;
            add = p.size() >= 5;
        }
        else if (iequals(entry, "buttontileart"))
        {
            e.type = ElementType::ButtonTileArt;
            e.x = n[1];
            e.y = n[2];
            e.graphic = parseU16(p.size() > 3 ? p[3] : "");
            e.graphicPressed = parseU16(p.size() > 4 ? p[4] : "");
            e.activates = n[5] != 0;
            e.toPage = n[6];
            e.id = static_cast<uint32_t>(n[7]);
            e.tileGraphic = parseU16(p.size() > 8 ? p[8] : "");
            e.tileHue = parseU16(p.size() > 9 ? p[9] : "");
            // The last two numbers are the button's size: the art is centred in
            // it and the whole area is clickable (see ButtonTileArt.cs).
            e.width = n[10];
            e.height = n[11];
            add = true;
        }
        else if (iequals(entry, "checkertrans"))
        {
            e.type = ElementType::CheckerTrans;
            e.x = n[1];
            e.y = n[2];
            e.width = n[3];
            e.height = n[4];
            add = true;
        }
        else if (iequals(entry, "croppedtext"))
        {
            e.type = ElementType::CroppedText;
            e.x = n[1];
            e.y = n[2];
            e.width = n[3];
            e.height = n[4];
            e.hue = textHue(parseU16(p.size() > 5 ? p[5] : ""));
            e.text = p.size() > 6 ? lineAt(lines, p[6]) : std::string{};
            add = p.size() > 6;
        }
        else if (iequals(entry, "gumppic") || iequals(entry, "tilepicasgumppic") || iequals(entry, "gumppichued") ||
                 iequals(entry, "gumppicphued"))
        {
            e.type = ElementType::GumpPic;
            e.x = n[1];
            e.y = n[2];
            e.graphic = parseU16(p.size() > 3 ? p[3] : "");

            if (p.size() > 4)
            {
                std::string_view h = p[4];

                if (auto eq = h.find('='); eq != std::string_view::npos)
                {
                    h = h.substr(eq + 1);
                }

                e.hue = gumpPicHue(parseU16(h));
            }

            e.partialHue = iequals(entry, "gumppicphued");
            e.virtue = p.size() >= 6 && icontains(p[5], "virtuegumpitem");
            // ClassicUO adds gumppichued/gumppicphued to page 0 (every page),
            // which is an oversight; they belong to the current page like gumppic.
            add = p.size() >= 4;
        }
        else if (iequals(entry, "gumppictiled"))
        {
            e.type = ElementType::GumpPicTiled;
            e.x = n[1];
            e.y = n[2];
            e.width = n[3];
            e.height = n[4];
            e.graphic = parseU16(p.size() > 5 ? p[5] : "");
            add = p.size() > 5;
        }
        else if (iequals(entry, "htmlgump"))
        {
            e.type = ElementType::Html;
            e.x = n[1];
            e.y = n[2];
            e.width = n[3];
            e.height = n[4];
            e.text = p.size() > 5 ? lineAt(lines, p[5]) : std::string{};
            e.background = p.size() > 6 && p[6] == "1";
            e.scroll = p.size() > 7 && p[7] != "0" ? (p[7] == "2" ? ScrollStyle::Flag : ScrollStyle::Bar)
                                                   : ScrollStyle::None;
            add = p.size() > 7;
        }
        else if (iequals(entry, "xmfhtmlgump") || iequals(entry, "xmfhtmlgumpcolor") || iequals(entry, "xmfhtmltok"))
        {
            // xmfhtmlgump      x y w h cliloc background scrollbar
            // xmfhtmlgumpcolor x y w h cliloc background scrollbar color
            // xmfhtmltok       x y w h background scrollbar color cliloc @args@
            const bool tok = iequals(entry, "xmfhtmltok");
            const bool colored = tok || iequals(entry, "xmfhtmlgumpcolor");
            const size_t bgIndex = tok ? 5 : 6;

            e.type = ElementType::XmfHtml;
            e.x = n[1];
            e.y = n[2];
            e.width = n[3];
            e.height = n[4];

            auto cliloc = parseCliloc(p.size() > (tok ? 8u : 5u) ? p[tok ? 8 : 5] : "");
            e.cliloc = cliloc.value_or(0);

            std::string_view bg = p.size() > bgIndex ? p[bgIndex] : "0";
            std::string_view sb = p.size() > bgIndex + 1 ? p[bgIndex + 1] : "0";
            e.background = n[bgIndex] == 1;
            // ClassicUO only uses the flag scrollbar here when a background is
            // also set; kept as is so these gumps look the same.
            e.scroll = n[bgIndex + 1] != 0 ? (bg != "0" && sb == "2" ? ScrollStyle::Flag : ScrollStyle::Bar)
                                           : ScrollStyle::None;

            if (colored)
            {
                int color = n[tok ? 7 : 8];
                e.htmlColor = color == 0x7FFF ? 0x00FFFFFF : static_cast<uint32_t>(color);
            }

            if (tok && p.size() >= 10)
            {
                std::string args = joinArgs(p, 9);
                // Arguments are "@a@b@" on some servers: trim and turn the rest into tabs.
                size_t a = args.find_first_not_of('@');
                size_t b = args.find_last_not_of('@');
                args = a == std::string::npos ? std::string{} : args.substr(a, b - a + 1);
                std::replace(args.begin(), args.end(), '@', '\t');
                e.clilocArgs = std::move(args);
            }

            add = cliloc.has_value();
        }
        else if (iequals(entry, "page"))
        {
            if (p.size() >= 2)
            {
                if (auto v = parseInt(p[1]))
                {
                    page = *v;
                }
            }
        }
        else if (iequals(entry, "resizepic"))
        {
            e.type = ElementType::ResizePic;
            e.x = n[1];
            e.y = n[2];
            e.graphic = parseU16(p.size() > 3 ? p[3] : "");
            e.width = n[4];
            e.height = n[5];
            add = true;
        }
        else if (iequals(entry, "text"))
        {
            e.type = ElementType::Text;
            e.x = n[1];
            e.y = n[2];
            e.hue = textHue(parseU16(p.size() > 3 ? p[3] : ""));
            e.text = p.size() > 4 ? lineAt(lines, p[4]) : std::string{};
            add = p.size() >= 5;
        }
        else if (iequals(entry, "textentry") || iequals(entry, "textentrylimited"))
        {
            e.type = ElementType::TextEntry;
            e.x = n[1];
            e.y = n[2];
            e.width = n[3];
            e.height = n[4];
            e.hue = textHue(parseU16(p.size() > 5 ? p[5] : ""));
            e.id = parseSerial(p.size() > 6 ? p[6] : "").value_or(0);
            e.text = p.size() > 7 ? lineAt(lines, p[7]) : std::string{};
            e.maxLength = iequals(entry, "textentrylimited") ? n[8] : 255;
            add = p.size() > 7;
        }
        else if (iequals(entry, "tilepic") || iequals(entry, "tilepichue"))
        {
            e.type = ElementType::TilePic;
            e.x = n[1];
            e.y = n[2];
            e.graphic = parseU16(p.size() > 3 ? p[3] : "");
            e.hue = p.size() > 4 ? parseU16(p[4]) : 0;
            add = p.size() > 3;
        }
        else if (iequals(entry, "noclose"))
        {
            g.canCloseWithRightClick = false;
        }
        else if (iequals(entry, "nodispose"))
        {
            g.canCloseWithEsc = false;
        }
        else if (iequals(entry, "nomove"))
        {
            g.canMove = false;
        }
        else if (iequals(entry, "group") || iequals(entry, "endgroup"))
        {
            ++group;
        }
        else if (iequals(entry, "radio") || iequals(entry, "checkbox"))
        {
            e.type = iequals(entry, "radio") ? ElementType::Radio : ElementType::Checkbox;
            e.group = group;
            e.x = n[1];
            e.y = n[2];
            e.graphic = parseU16(p.size() > 3 ? p[3] : "");
            e.graphicPressed = parseU16(p.size() > 4 ? p[4] : "");
            e.checked = p.size() > 5 && p[5] == "1";
            e.id = parseSerial(p.size() > 6 ? p[6] : "").value_or(0);
            add = p.size() > 6;
        }
        else if (iequals(entry, "tooltip"))
        {
            auto cliloc = parseCliloc(p.size() > 1 ? p[1] : "");

            if (cliloc && !g.elements.empty())
            {
                TooltipLine line;
                line.cliloc = *cliloc;

                if (p.size() > 2 && !p[2].empty())
                {
                    line.args = joinArgs(p, 2);
                }

                auto& last = g.elements.back();
                last.tooltips.push_back(std::move(line));
            }
        }
        else if (iequals(entry, "itemproperty"))
        {
            if (!g.elements.empty() && p.size() > 1)
            {
                g.elements.back().itemProperty = parseSerial(p[1]);
            }
        }
        else if (iequals(entry, "mastergump"))
        {
            g.masterGump = p.size() > 1 ? parseSerial(p[1]).value_or(0) : 0;
        }
        else if (iequals(entry, "picinpic") || iequals(entry, "picinpichued") || iequals(entry, "picinpicphued"))
        {
            e.type = ElementType::PicInPic;
            e.x = n[1];
            e.y = n[2];
            e.graphic = parseU16(p.size() > 3 ? p[3] : "");
            e.srcX = parseU16(p.size() > 4 ? p[4] : "");
            e.srcY = parseU16(p.size() > 5 ? p[5] : "");
            e.width = parseU16(p.size() > 6 ? p[6] : "");
            e.height = parseU16(p.size() > 7 ? p[7] : "");

            if (p.size() > 8)
            {
                e.hue = parseU16(p[8]);
                e.partialHue = iequals(entry, "picinpicphued");
            }

            add = p.size() > 7;
        }
        else if (iequals(entry, "noresize") || iequals(entry, "togglelimitgumpscale"))
        {
            // Accepted and ignored, as in ClassicUO.
        }
        else
        {
            g.unknownCommands.emplace_back(entry);
        }

        if (add && n.ok)
        {
            g.elements.push_back(std::move(e));
        }
    }

    return g;
}

}  // namespace uo::gumps
