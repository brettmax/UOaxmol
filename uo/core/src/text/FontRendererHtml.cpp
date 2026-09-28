// SPDX-License-Identifier: BSD-2-Clause
// Ported from ClassicUO (FontsLoader.cs): the HTML half of the unicode font renderer.
// Parsing keeps the original's control flow, quirks included, so gump HTML lays out and
// colors exactly as it did. Where the original threw (a malformed number in a margin or a
// color) the attribute is ignored instead. Tag and attribute names compare ASCII
// case-insensitively; the original's culture-aware comparison also equated a few exotic
// code points (the Kelvin sign, zero-width characters), which gump text does not use.

#include "uo/text/FontRenderer.h"

#include "uo/text/Utf.h"

#include <algorithm>
#include <cstdint>
#include <limits>

namespace uo::text
{

namespace
{
constexpr int kMaxHtmlTextHeight = 18;

enum class HtmlTag : uint8_t
{
    None = 0,
    B,
    I,
    A,
    U,
    P,
    Big,
    Small,
    Body,
    BaseFont,
    H1,
    H2,
    H3,
    H4,
    H5,
    H6,
    Br,
    Bq,
    Left,
    Center,
    Right,
    Div,
    BodyBgColor,
};

char16_t lowerAscii(char16_t c)
{
    return c >= u'A' && c <= u'Z' ? static_cast<char16_t>(c + 32) : c;
}

bool equalsNoCase(std::u16string_view a, std::string_view b)
{
    if (a.size() != b.size())
        return false;

    for (size_t i = 0; i < a.size(); i++)
        if (lowerAscii(a[i]) != static_cast<char16_t>(b[i]))
            return false;

    return true;
}

int indexOfNoCase(std::u16string_view hay, std::string_view needle)
{
    if (needle.size() > hay.size())
        return -1;

    for (size_t i = 0; i + needle.size() <= hay.size(); i++)
        if (equalsNoCase(hay.substr(i, needle.size()), needle))
            return static_cast<int>(i);

    return -1;
}

// .NET char.IsWhiteSpace over the BMP.
bool isWhiteSpace(char16_t c)
{
    return (c >= 0x09 && c <= 0x0D) || c == 0x20 || c == 0x85 || c == 0xA0 || c == 0x1680 ||
           (c >= 0x2000 && c <= 0x200A) || c == 0x2028 || c == 0x2029 || c == 0x202F || c == 0x205F ||
           c == 0x3000;
}

// Whitespace .NET number parsing skips around a number.
bool isNumberWhite(char16_t c)
{
    return (c >= 0x09 && c <= 0x0D) || c == 0x20;
}

void trimWhitespace(std::u16string_view& s)
{
    while (!s.empty() && isWhiteSpace(s.front()))
        s.remove_prefix(1);
}

// Trims what .NET's integer parsing tolerates around a number (white space, and trailing NULs).
std::u16string_view numberBody(std::u16string_view s)
{
    while (!s.empty() && isNumberWhite(s.front()))
        s.remove_prefix(1);

    while (!s.empty() && (isNumberWhite(s.back()) || s.back() == 0))
        s.remove_suffix(1);

    return s;
}

// int.Parse / byte.TryParse with NumberStyles.Integer.
bool parseInteger(std::u16string_view s, int64_t min, int64_t max, int64_t& out)
{
    s = numberBody(s);
    bool negative = false;

    if (!s.empty() && (s.front() == u'+' || s.front() == u'-'))
    {
        negative = s.front() == u'-';
        s.remove_prefix(1);
    }

    if (s.empty())
        return false;

    int64_t value = 0;

    for (char16_t c : s)
    {
        if (c < u'0' || c > u'9')
            return false;

        value = value * 10 + (c - u'0');

        if (value > max + 1)
            return false;
    }

    if (negative)
        value = -value;

    if (value < min || value > max)
        return false;

    out = value;
    return true;
}

int hexDigit(char16_t c)
{
    if (c >= u'0' && c <= u'9')
        return c - u'0';
    if (c >= u'a' && c <= u'f')
        return c - u'a' + 10;
    if (c >= u'A' && c <= u'F')
        return c - u'A' + 10;
    return -1;
}

bool parseHexDigits(std::u16string_view s, uint32_t& out)
{
    if (s.empty())
        return false;

    uint64_t value = 0;

    for (char16_t c : s)
    {
        const int d = hexDigit(c);

        if (d < 0)
            return false;

        value = (value << 4) | static_cast<uint64_t>(d);

        if (value > std::numeric_limits<uint32_t>::max())
            return false;
    }

    out = static_cast<uint32_t>(value);
    return true;
}

// uint.TryParse(s, NumberStyles.HexNumber); 0 on failure.
uint32_t tryParseHex(std::u16string_view s)
{
    uint32_t v = 0;
    return parseHexDigits(numberBody(s), v) ? v : 0;
}

// Convert.ToUInt32(s, 16), which accepts a 0x prefix.
bool convertHex(std::u16string_view s, uint32_t& out)
{
    if (s.size() >= 2 && s[0] == u'0' && (s[1] == u'x' || s[1] == u'X'))
        s.remove_prefix(2);

    return parseHexDigits(s, out);
}

// ReadColorFromTextBuffer: "#rrggbb", a bare hex number, or a color name.
uint32_t readColor(std::u16string_view buffer)
{
    uint32_t color = 0;

    if (buffer.empty())
        return color;

    if (buffer[0] == u'#')
    {
        if (buffer.size() > 1)
        {
            // The original read buffer[2] unchecked; "#0" alone faulted there.
            const size_t start = buffer.size() > 2 && buffer[1] == u'0' && buffer[2] == u'x' ? 3 : 1;
            const uint32_t cc  = tryParseHex(buffer.substr(start));
            color = ((cc & 0xFF) << 24) | (((cc >> 8) & 0xFF) << 16) | (((cc >> 16) & 0xFF) << 8) | 0xFF;
        }
    }
    else if (buffer[0] >= u'0' && buffer[0] <= u'9')
    {
        // The original threw on a malformed number; it reads as no color here.
        if (!convertHex(buffer, color))
            color = 0;
    }
    else
    {
        struct Named
        {
            const char* name;
            uint32_t color;
        };
        static constexpr Named kNamed[] = {
            {"red", 0x0000FFFF},     {"cyan", 0xFFFF00FF},  {"blue", 0xFF0000FF},    {"darkblue", 0xA00000FF},
            {"lightblue", 0xE6D8ADFF}, {"purple", 0x800080FF}, {"yellow", 0x00FFFFFF}, {"lime", 0x00FF00FF},
            {"magenta", 0xFF00FFFF}, {"white", 0xFFFEFEFF}, {"silver", 0xC0C0C0FF},  {"grey", 0x808080FF},
            {"gray", 0x808080FF},    {"black", 0x010101FF}, {"orange", 0x00A5FFFF},  {"brown", 0x2A2AA5FF},
            {"maroon", 0x000080FF},  {"green", 0x008000FF}, {"olive", 0x008080FF},
        };

        for (const auto& n : kNamed)
            if (equalsNoCase(buffer, n.name))
                return n.color;
    }

    return color;
}

TextLine& newLine(TextLayout& lines)
{
    lines.emplace_back();
    return lines.back();
}

void setDataCount(std::vector<GlyphRun>& data, int target)
{
    data.resize(static_cast<size_t>(std::max(target, 0)));
}
}  // namespace

struct FontRenderer::HtmlChar
{
    char16_t ch     = 0;
    uint8_t font    = 0;
    TextAlign align = TextAlign::Left;
    uint16_t flags  = 0;
    uint32_t color  = 0;
    uint16_t linkId = 0;
};

struct FontRenderer::HtmlTagInfo
{
    HtmlTag tag     = HtmlTag::None;
    TextAlign align = TextAlign::Left;
    uint16_t flags  = 0;
    uint8_t font    = 0;
    uint32_t color  = 0;
    uint16_t link   = 0;
};

// ---------------------------------------------------------------------------
// Mode and visited links
// ---------------------------------------------------------------------------

void FontRenderer::setUseHtml(bool value, uint32_t startColor, bool backgroundCanBeColored)
{
    _useHtml                 = value;
    _html.color              = startColor;
    _html.backgroundColored  = backgroundCanBeColored;
}

FontRenderer::HtmlScope::HtmlScope(FontRenderer& fonts, uint32_t startColor, bool backgroundCanBeColored)
    : _fonts(fonts), _wasHtml(fonts._useHtml), _oldColor(fonts._html.color),
      _oldBackground(fonts._html.backgroundColored)
{
    _fonts.setUseHtml(true, startColor, backgroundCanBeColored);
}

FontRenderer::HtmlScope::~HtmlScope()
{
    _fonts.setUseHtml(_wasHtml, _oldColor, _oldBackground);
}

bool FontRenderer::VisitedUrls::isVisited(const std::string& url)
{
    auto it = _map.find(url);

    if (it == _map.end())
        return false;

    _order.splice(_order.begin(), _order, it->second);
    return true;
}

void FontRenderer::VisitedUrls::mark(const std::string& url)
{
    if (isVisited(url))
        return;

    if (_map.size() >= kCapacity)
    {
        _map.erase(_order.back());
        _order.pop_back();
    }

    _order.push_front(url);
    _map[url] = _order.begin();
}

void FontRenderer::markUrlVisited(std::string_view url)
{
    if (!url.empty())
        _visitedUrls.mark(std::string(url));
}

bool FontRenderer::urlVisited(std::string_view url) const
{
    return _visitedUrls.contains(std::string(url));
}

void FontRenderer::resetHtmlStatus() const
{
    _html.webLinkColor        = 0xFF0000FF;
    _html.visitedWebLinkColor = 0x0000FFFF;
    _html.backgroundColor     = 0;
    _html.margins             = {};
}

// ---------------------------------------------------------------------------
// Parsing
// ---------------------------------------------------------------------------

int FontRenderer::htmlVisibleLength(uint8_t font, std::u16string_view str) const
{
    std::vector<HtmlChar> data;
    return htmlData(data, font, str, TextAlign::Left, 0, nullptr);
}

void FontRenderer::htmlTextByWidthPrefix(uint8_t font, std::u16string_view& str, int width, bool& isCropped,
                                         std::u16string& out, bool unicode) const
{
    const int strLen = htmlVisibleLength(font, str);
    const int size   = static_cast<int>(str.size()) - strLen;

    if (size <= 0)
        return;

    out.append(str.substr(0, static_cast<size_t>(size)));
    str = str.substr(str.size() - static_cast<size_t>(strLen));

    if ((unicode ? widthUnicode(font, str) : widthAscii(font, str)) < width)
        isCropped = false;
}

int FontRenderer::htmlData(std::vector<HtmlChar>& data, uint8_t font, std::u16string_view str, TextAlign align,
                           uint16_t flags, std::vector<std::string>* urls) const
{
    const int len = static_cast<int>(str.size());
    data.assign(str.size(), HtmlChar{});
    int newlen = 0;

    HtmlTagInfo info{HtmlTag::None, align, flags, font, _html.color, 0};
    std::vector<HtmlTagInfo> stack;
    stack.reserve(8);
    stack.push_back(info);
    HtmlTagInfo currentInfo = info;

    for (int i = 0; i < len; i++)
    {
        char16_t si = str[i];

        if (si == u'<')
        {
            bool endTag = false;
            HtmlTagInfo newInfo{HtmlTag::None, TextAlign::Left, 0, 0xFF, 0, 0};

            const auto tag = static_cast<HtmlTag>(parseHtmlTag(str, len, i, endTag, newInfo, urls));

            if (tag == HtmlTag::None)
                continue;

            if (!endTag)
            {
                if (newInfo.font == 0xFF)
                    newInfo.font = stack.back().font;

                if (tag != HtmlTag::Body)
                {
                    stack.push_back(newInfo);
                }
                else
                {
                    stack.clear();
                    newlen = 0;

                    if (newInfo.color != 0)
                        info.color = newInfo.color;

                    stack.push_back(info);
                }
            }
            else if (stack.size() > 1)
            {
                for (size_t j = stack.size() - 1; j >= 1; j--)
                {
                    if (stack[j].tag == tag)
                    {
                        stack.erase(stack.begin() + static_cast<std::ptrdiff_t>(j));
                        break;
                    }
                }
            }

            currentHtmlInfo(stack, currentInfo);

            switch (tag)
            {
            case HtmlTag::Left:
            case HtmlTag::Center:
            case HtmlTag::Right:
                if (newlen != 0)
                    endTag = true;
                [[fallthrough]];
            case HtmlTag::P:
                si = endTag ? u'\n' : 0;
                break;
            case HtmlTag::BodyBgColor:
            case HtmlTag::Br:
            case HtmlTag::Bq:
                si = u'\n';
                break;
            default:
                si = 0;
                break;
            }
        }

        if (si != 0)
        {
            HtmlChar& c = data[static_cast<size_t>(newlen)];
            c.ch        = si;
            c.font      = currentInfo.font;
            c.align     = currentInfo.align;
            c.flags     = currentInfo.flags;
            c.color     = currentInfo.color;
            c.linkId    = currentInfo.link;
            ++newlen;
        }
    }

    data.resize(static_cast<size_t>(newlen));
    return newlen;
}

void FontRenderer::currentHtmlInfo(const std::vector<HtmlTagInfo>& stack, HtmlTagInfo& info) const
{
    info = HtmlTagInfo{HtmlTag::None, TextAlign::Left, 0, 0xFF, 0, 0};

    for (const HtmlTagInfo& current : stack)
    {
        switch (current.tag)
        {
        case HtmlTag::None:
            info = current;
            break;

        case HtmlTag::B:
        case HtmlTag::I:
        case HtmlTag::U:
        case HtmlTag::P:
            info.flags |= current.flags;
            info.align = current.align;
            break;

        case HtmlTag::A:
            info.flags |= current.flags;
            info.color = current.color;
            info.link  = current.link;
            break;

        case HtmlTag::Big:
        case HtmlTag::Small:
            if (current.font != 0xFF && unicodeFontExists(current.font))
                info.font = current.font;
            break;

        case HtmlTag::BaseFont:
            if (current.font != 0xFF && unicodeFontExists(current.font))
                info.font = current.font;
            if (current.color != 0)
                info.color = current.color;
            break;

        case HtmlTag::H1:
        case HtmlTag::H2:
        case HtmlTag::H4:
        case HtmlTag::H5:
            info.flags |= current.flags;
            [[fallthrough]];
        case HtmlTag::H3:
        case HtmlTag::H6:
            if (current.font != 0xFF && unicodeFontExists(current.font))
                info.font = current.font;
            break;

        case HtmlTag::Bq:
            info.color = current.color;
            info.flags |= current.flags;
            break;

        case HtmlTag::Left:
        case HtmlTag::Center:
        case HtmlTag::Right:
        case HtmlTag::Div:
            info.align = current.align;
            break;

        default:
            break;
        }
    }
}

namespace
{
// GetHTMLInfoFromTag: a tag's own style before its attributes.
void htmlInfoFromTag(HtmlTag tag, uint16_t& flags, uint8_t& font, uint32_t& color, TextAlign& align)
{
    align = TextAlign::Left;
    flags = 0;
    font  = 0xFF;
    color = 0;

    switch (tag)
    {
    case HtmlTag::B: flags = FontStyleSolid; break;
    case HtmlTag::I: flags = FontStyleItalic; break;
    case HtmlTag::U: flags = FontStyleUnderline; break;
    case HtmlTag::P: flags = FontStyleIndention; break;
    case HtmlTag::Big: font = 0; break;
    case HtmlTag::Small: font = 2; break;
    case HtmlTag::H1:
        flags = FontStyleSolid | FontStyleUnderline;
        font  = 0;
        break;
    case HtmlTag::H2:
        flags = FontStyleSolid;
        font  = 0;
        break;
    case HtmlTag::H3: font = 0; break;
    case HtmlTag::H4:
        flags = FontStyleSolid;
        font  = 2;
        break;
    case HtmlTag::H5:
        flags = FontStyleItalic;
        font  = 2;
        break;
    case HtmlTag::H6: font = 2; break;
    case HtmlTag::Bq:
        flags = FontStyleBQ;
        color = 0x008000FF;
        break;
    case HtmlTag::Left: align = TextAlign::Left; break;
    case HtmlTag::Center: align = TextAlign::Center; break;
    case HtmlTag::Right: align = TextAlign::Right; break;
    default: break;
    }
}

HtmlTag tagFromName(std::u16string_view name)
{
    struct Entry
    {
        const char* name;
        HtmlTag tag;
    };
    static constexpr Entry kTags[] = {
        {"b", HtmlTag::B},         {"i", HtmlTag::I},           {"a", HtmlTag::A},
        {"u", HtmlTag::U},         {"p", HtmlTag::P},           {"big", HtmlTag::Big},
        {"small", HtmlTag::Small}, {"body", HtmlTag::Body},     {"basefont", HtmlTag::BaseFont},
        {"h1", HtmlTag::H1},       {"h2", HtmlTag::H2},         {"h3", HtmlTag::H3},
        {"h4", HtmlTag::H4},       {"h5", HtmlTag::H5},         {"h6", HtmlTag::H6},
        {"br", HtmlTag::Br},       {"bq", HtmlTag::Bq},         {"left", HtmlTag::Left},
        {"center", HtmlTag::Center}, {"right", HtmlTag::Right}, {"div", HtmlTag::Div},
    };

    for (const auto& e : kTags)
        if (equalsNoCase(name, e.name))
            return e.tag;

    return HtmlTag::None;
}
}  // namespace

int FontRenderer::parseHtmlTag(std::u16string_view str, int len, int& i, bool& endTag, HtmlTagInfo& info,
                               std::vector<std::string>* urls) const
{
    HtmlTag tag = HtmlTag::None;
    i++;

    if (i < len && str[i] == u'/')
    {
        endTag = true;
        i++;
    }

    while (i < len && str[i] == u' ')
        i++;

    int j = i;

    for (; i < len; i++)
    {
        // A self-closing <tag/>.
        if (str[i] == u'/')
        {
            endTag = true;
            break;
        }

        if (str[i] == u' ' || str[i] == u'>')
            break;
    }

    if (j != i && i < len)
    {
        int cmdLen           = i - j;
        const int startIndex = j;
        j                    = i;

        while (i < len && str[i] != u'>')
            i++;

        tag = tagFromName(str.substr(static_cast<size_t>(startIndex), static_cast<size_t>(cmdLen)));

        if (tag == HtmlTag::None)
        {
            // The original searches the whole string, not just this tag.
            if (indexOfNoCase(str, "bodybgcolor") >= 0)
            {
                tag    = HtmlTag::BodyBgColor;
                j      = indexOfNoCase(str, "bgcolor");
                endTag = false;
            }
            else if (indexOfNoCase(str, "basefont") >= 0)
            {
                tag    = HtmlTag::BaseFont;
                j      = indexOfNoCase(str, "color");
                endTag = false;
            }
            else if (indexOfNoCase(str, "bodytext") >= 0)
            {
                tag    = HtmlTag::Body;
                j      = indexOfNoCase(str, "text");
                endTag = false;
            }
        }

        if (!endTag)
        {
            info.tag = tag;
            info.link = 0;
            htmlInfoFromTag(tag, info.flags, info.font, info.color, info.align);

            if (i < len && j != i)
            {
                switch (tag)
                {
                case HtmlTag::BodyBgColor:
                case HtmlTag::Body:
                case HtmlTag::BaseFont:
                case HtmlTag::A:
                case HtmlTag::Div:
                case HtmlTag::P:
                    cmdLen = i - j;

                    // j is -1 when the fallback's attribute name is missing; the original
                    // threw there.
                    if (!str.empty() && cmdLen >= 0 && j >= 0 && static_cast<int>(str.size()) > j &&
                        static_cast<int>(str.size()) >= cmdLen)
                        htmlInfoFromContent(info, str.substr(static_cast<size_t>(j), static_cast<size_t>(cmdLen)),
                                            urls);
                    break;
                default:
                    break;
                }
            }
        }
    }

    return static_cast<int>(tag);
}

void FontRenderer::htmlInfoFromContent(HtmlTagInfo& info, std::u16string_view content,
                                       std::vector<std::string>* urls) const
{
    while (!content.empty())
    {
        trimWhitespace(content);

        if (content.empty())
            break;

        std::u16string_view command;

        for (size_t i = 0; i < content.size(); i++)
        {
            const char16_t c = content[i];

            if (isWhiteSpace(c) || c == u'=' || c == u'\\')
            {
                command = content.substr(0, i);
                content = content.substr(i);
                break;
            }
        }

        if (command.empty())
            break;

        trimWhitespace(content);

        std::u16string_view value;

        if (!content.empty() && content[0] == u'=')
        {
            content.remove_prefix(1);
            trimWhitespace(content);

            if (!content.empty() && content[0] == u'"')
            {
                const size_t q = content.substr(1).find(u'"');

                if (q != std::u16string_view::npos)
                {
                    const size_t endQuote = q + 1;
                    value                 = content.substr(1, endQuote - 1);
                    content               = content.substr(endQuote + 1);
                }
            }
            else
            {
                size_t i = 0;

                for (; i < content.size(); i++)
                {
                    const char16_t c = content[i];

                    if (isWhiteSpace(c) || c == u'\\' || c == u'<' || c == u'>' || c == u'=')
                        break;
                }

                value   = content.substr(0, i);
                content = content.substr(i);
            }
        }

        int64_t number = 0;

        switch (info.tag)
        {
        case HtmlTag::Body:
        case HtmlTag::BodyBgColor:
            if (equalsNoCase(command, "text"))
                info.color = readColor(value);
            else if (equalsNoCase(command, "bgcolor"))
            {
                if (_html.backgroundColored)
                    _html.backgroundColor = readColor(value);
            }
            else if (equalsNoCase(command, "link"))
                _html.webLinkColor = readColor(value);
            else if (equalsNoCase(command, "vlink"))
                _html.visitedWebLinkColor = readColor(value);
            else if (equalsNoCase(command, "leftmargin"))
            {
                if (parseInteger(value, INT32_MIN, INT32_MAX, number))
                    _html.margins.x = static_cast<int>(number);
            }
            else if (equalsNoCase(command, "topmargin"))
            {
                if (parseInteger(value, INT32_MIN, INT32_MAX, number))
                    _html.margins.y = static_cast<int>(number);
            }
            else if (equalsNoCase(command, "rightmargin"))
            {
                if (parseInteger(value, INT32_MIN, INT32_MAX, number))
                    _html.margins.width = static_cast<int>(number);
            }
            else if (equalsNoCase(command, "bottommargin"))
            {
                if (parseInteger(value, INT32_MIN, INT32_MAX, number))
                    _html.margins.height = static_cast<int>(number);
            }
            break;

        case HtmlTag::BaseFont:
            if (equalsNoCase(command, "color"))
            {
                info.color = readColor(value);
            }
            else if (equalsNoCase(command, "size"))
            {
                if (!parseInteger(value, 0, 255, number))
                {
                    if (equalsNoCase(value, "big"))
                        info.font = 4;
                    else if (equalsNoCase(value, "small"))
                        info.font = 0;
                    else
                        info.font = 1;
                }
                else if (number == 0 || number == 4)
                    info.font = 1;
                else if (number < 4)
                    info.font = 2;
                else
                    info.font = 0;
            }
            break;

        case HtmlTag::A:
            if (equalsNoCase(command, "href"))
            {
                info.flags = FontStyleUnderline;
                info.color = _html.webLinkColor;
                info.link  = registerParseUrl(urls, value, info.color);
            }
            break;

        case HtmlTag::P:
        case HtmlTag::Div:
            if (equalsNoCase(command, "align"))
            {
                if (equalsNoCase(value, "left"))
                    info.align = TextAlign::Left;
                else if (equalsNoCase(value, "center"))
                    info.align = TextAlign::Center;
                else if (equalsNoCase(value, "right"))
                    info.align = TextAlign::Right;
            }
            break;

        default:
            break;
        }

        trimWhitespace(content);
    }
}

uint16_t FontRenderer::registerParseUrl(std::vector<std::string>* urls, std::u16string_view link,
                                        uint32_t& color) const
{
    // Measuring passes collect no links.
    if (!urls)
        return 0;

    std::string url = utf16ToUtf8(link);

    if (_visitedUrls.isVisited(url))
        color = _html.visitedWebLinkColor;

    if (urls->size() >= 0xFFFF)
        return 0;

    urls->push_back(std::move(url));
    return static_cast<uint16_t>(urls->size());
}

// ---------------------------------------------------------------------------
// Layout (GetInfoHTML)
// ---------------------------------------------------------------------------

TextLayout FontRenderer::layoutHtml(uint8_t font, std::u16string_view str, TextAlign align, uint16_t flags,
                                    int width, std::vector<std::string>& urls) const
{
    TextLayout lines;

    if (str.empty())
        return lines;

    std::vector<HtmlChar> htmlData;
    const int len = this->htmlData(htmlData, font, str, align, flags, &urls);

    if (len <= 0)
        return lines;

    TextLine* ptr       = &newLine(lines);
    ptr->align          = align;
    int indentionOffset = 0;
    ptr->indentionOffset = indentionOffset;
    int charCount       = 0;
    int lastSpace       = 0;
    int readWidth       = 0;
    const bool isFixed   = (flags & FontStyleFixed) != 0;
    const bool isCropped = (flags & FontStyleCropped) != 0;

    ptr->align = htmlData[0].align;

    for (int i = 0; i < len; i++)
    {
        char16_t si           = htmlData[i].ch;
        const UnicodeGlyph& g = unicodeGlyph(htmlData[i].font, si);

        if (si == u'\r' || si == u'\n')
            si = (si == u'\r' || isFixed || isCropped) ? 0 : u'\n';

        if (!g.data && si != u' ' && si != u'\n')
            continue;

        if (si == u' ')
        {
            lastSpace = i;
            ptr->width += readWidth;
            readWidth = 0;
            ptr->charCount += charCount;
            charCount = 0;
        }

        int solidWidth        = htmlData[i].flags & FontStyleSolid;
        const int charWidth   = g.offsetX + g.width + 1;

        if (ptr->width + readWidth + charWidth + solidWidth > width || si == u'\n')
        {
            if (lastSpace == ptr->charStart && lastSpace == 0 && si != u'\n')
                ptr->charStart = 1;

            if (si == u'\n')
            {
                ptr->width += readWidth;
                ptr->charCount += charCount;
                lastSpace = i;

                if (ptr->width <= 0)
                    ptr->width = 1;

                ptr->maxHeight = kMaxHtmlTextHeight;
                setDataCount(ptr->data, ptr->charCount);

                ptr        = &newLine(lines);
                ptr->align = htmlData[i].align;
                ptr->charStart = i + 1;
                readWidth      = 0;
                charCount      = 0;
                indentionOffset      = 0;
                ptr->indentionOffset = indentionOffset;
                continue;
            }

            if (lastSpace + 1 == ptr->charStart && !isFixed && !isCropped)
            {
                ptr->width += readWidth;
                ptr->charCount += charCount;

                if (ptr->width <= 0)
                    ptr->width = 1;

                ptr->maxHeight = kMaxHtmlTextHeight;

                ptr            = &newLine(lines);
                ptr->align     = htmlData[i].align;
                ptr->charStart = i;
                lastSpace      = i - 1;
                charCount      = 0;

                if (ptr->align == TextAlign::Left && (htmlData[i].flags & FontStyleIndention) != 0)
                    indentionOffset = 14;

                ptr->indentionOffset = indentionOffset;
                readWidth            = indentionOffset;
            }
            else
            {
                if (isFixed)
                {
                    const HtmlChar& h = htmlData[i];
                    ptr->data.push_back({h.color, h.flags, h.font, si, h.linkId});
                    readWidth += charWidth;
                    ptr->maxHeight = kMaxHtmlTextHeight;
                    charCount++;
                    ptr->width += readWidth;
                    ptr->charCount += charCount;
                }
                else if (isCropped)
                {
                    ptr->width += readWidth;
                    ptr->charCount += charCount;
                }

                i = lastSpace + 1;

                if (i >= len)
                    break;

                // Like the plain path, the rewound character keeps the overflowing one's glyph.
                si         = htmlData[i].ch;
                solidWidth = htmlData[i].flags & FontStyleSolid;

                if (ptr->width <= 0)
                    ptr->width = 1;

                ptr->maxHeight = kMaxHtmlTextHeight;
                setDataCount(ptr->data, ptr->charCount);
                charCount = 0;

                if (isFixed || isCropped)
                    break;

                ptr            = &newLine(lines);
                ptr->align     = htmlData[i].align;
                ptr->charStart = i;

                if (ptr->align == TextAlign::Left && (htmlData[i].flags & FontStyleIndention) != 0)
                    indentionOffset = 14;

                ptr->indentionOffset = indentionOffset;
                readWidth            = indentionOffset;
            }
        }

        const HtmlChar& h = htmlData[i];
        ptr->data.push_back({h.color, h.flags, h.font, si, h.linkId});

        if (si == u' ')
            readWidth += kUnicodeSpaceWidth;
        else
            readWidth += charWidth + solidWidth;

        charCount++;
    }

    ptr->width += readWidth;
    ptr->charCount += charCount;
    ptr->maxHeight = kMaxHtmlTextHeight;

    return lines;
}

}  // namespace uo::text
