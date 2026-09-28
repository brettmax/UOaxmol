// SPDX-License-Identifier: BSD-2-Clause
#include "gumps/ServerGump.h"

#include "uo/gumps/GumpPackets.h"

namespace uo::client::gumps
{

using uo::gumps::ElementType;
using uo::gumps::GumpElement;
using uo::gumps::GumpLayout;
using uo::gumps::ScrollStyle;

ServerGump::ServerGump(GumpContext& ctx, uint32_t sender, uint32_t gumpId)
    : Gump(ctx, GumpKind::Server, sender, gumpId)
{
    init();
    autorelease();
}

ServerGump* ServerGump::create(GumpContext& ctx, const GumpLayout& layout)
{
    auto* g = new ServerGump(ctx, layout.sender, layout.gumpId);
    g->rebuild(layout);
    return g;
}

void ServerGump::rebuild(const GumpLayout& layout)
{
    clearControls();
    _textFocused = false;

    setCanMove(layout.canMove);
    setCanCloseWithRightClick(layout.canCloseWithRightClick);
    setCanCloseWithEsc(layout.canCloseWithEsc);
    _masterGump = layout.masterGump;

    for (const GumpElement& e : layout.elements)
    {
        if (e.type == ElementType::CheckerTrans)
        {
            applyCheckerTrans(e);
        }

        Control* c = makeControl(e);

        if (!c)
        {
            continue;
        }

        c->setUOPosition(static_cast<float>(e.x), static_cast<float>(e.y));

        for (const auto& t : e.tooltips)
        {
            std::string line = context().text->cliloc(t.cliloc, t.args);

            if (!line.empty())
            {
                c->appendTooltip(line);
            }
        }

        if (!e.tooltips.empty() || e.itemProperty)
        {
            // A tooltip makes the control take the pointer, like ClassicUO (AcceptMouseInput).
            c->setAcceptsInput(true);
        }

        if (e.itemProperty)
        {
            c->setTooltipSerial(*e.itemProperty);
        }

        addControl(c, e.page);
    }

    // ClassicUO: a gump with page 0 content only starts on page 1.
    setActivePage(1);
}

Control* ServerGump::makeControl(const GumpElement& e)
{
    GumpContext& ctx = context();
    const float w = static_cast<float>(e.width);
    const float h = static_cast<float>(e.height);

    switch (e.type)
    {
    case ElementType::Button:
    {
        auto* b = new GumpButton(ctx, e.graphic, e.graphicPressed);
        b->setButtonId(e.id);
        b->setActivates(e.activates);
        b->setToPage(e.toPage);
        return b;
    }
    case ElementType::ButtonTileArt:
    {
        auto* b = new ButtonTileArt(ctx, e.graphic, e.graphicPressed, e.tileGraphic, e.tileHue, w, h);
        b->setButtonId(e.id);
        b->setActivates(e.activates);
        b->setToPage(e.toPage);
        return b;
    }
    case ElementType::CheckerTrans:
        return new CheckerTrans(w, h);
    case ElementType::CroppedText:
    {
        GumpTextStyle style;
        style.hue = e.hue;
        style.maxWidth = e.width;
        style.cropped = true;
        return new TextLabel(ctx, e.text, style, w, h);
    }
    case ElementType::GumpPic:
    {
        auto* p = new GumpPic(ctx, e.graphic, e.hue, e.partialHue);

        if (e.virtue)
        {
            p->setContainsByBounds(true);
            p->setTooltip(virtueTooltip(e.graphic, e.hue));
        }
        else
        {
            // Plain art takes no clicks unless a tooltip is attached (set by rebuild).
            p->setAcceptsInput(false);
        }

        return p;
    }
    case ElementType::GumpPicTiled:
    {
        auto* p = new GumpPicTiled(ctx, e.graphic, w, h);
        p->setAcceptsInput(false);
        return p;
    }
    case ElementType::PicInPic:
    {
        auto* p = new GumpPicInPic(ctx, e.graphic, static_cast<float>(e.srcX), static_cast<float>(e.srcY), w, h,
                                   e.hue, e.partialHue);
        p->setAcceptsInput(false);
        return p;
    }
    case ElementType::Html:
    case ElementType::XmfHtml:
    {
        std::string text = e.type == ElementType::Html ? e.text : ctx.text->cliloc(e.cliloc, e.clilocArgs);
        int scroll = e.scroll == ScrollStyle::None ? 0 : e.scroll == ScrollStyle::Bar ? 1 : 2;
        auto* html = new HtmlArea(ctx, text, w, h, e.background, scroll, e.htmlColor);

        if (scroll == 0)
        {
            html->setAcceptsInput(false);
        }

        return html;
    }
    case ElementType::ResizePic:
    {
        auto* p = new ResizePic(ctx, e.graphic, w, h);
        p->setAcceptsInput(false);
        return p;
    }
    case ElementType::Text:
    {
        GumpTextStyle style;
        style.hue = e.hue;
        auto* t = new TextLabel(ctx, e.text, style);
        return t;
    }
    case ElementType::TextEntry:
    {
        auto* t = new TextEntry(ctx, w, h, e.hue, e.text, e.maxLength);
        t->setLocalSerial(e.id);

        if (!_textFocused)
        {
            // The first entry takes the keyboard, as in ClassicUO.
            _textFocused = true;
            t->scheduleOnce([t](float) { t->focus(); }, 0, "focus");
        }

        return t;
    }
    case ElementType::TilePic:
    {
        auto* p = new StaticPic(ctx, e.graphic, e.hue);
        p->setAcceptsInput(false);
        return p;
    }
    case ElementType::Checkbox:
    case ElementType::Radio:
    {
        auto* c = new Checkbox(ctx, e.graphic, e.graphicPressed, e.checked, e.type == ElementType::Radio, e.group);
        c->setLocalSerial(e.id);
        return c;
    }
    }

    return nullptr;
}

void ServerGump::applyCheckerTrans(const GumpElement& e)
{
    // Half transparency for what is already under the rectangle on page 0 or the page being
    // built (ClassicUO's ApplyTrans).
    const ax::Rect area(static_cast<float>(e.x), static_cast<float>(e.y), static_cast<float>(e.width),
                        static_cast<float>(e.height));

    for (auto* c : controls())
    {
        if (c->page() != 0 && c->page() != e.page)
        {
            continue;
        }

        ax::Rect r(c->uoX(), c->uoY(), c->getContentSize().width, c->getContentSize().height);

        if (area.intersectsRect(r))
        {
            c->setCascadeOpacityEnabled(true);
            c->setOpacity(128);
        }
    }
}

std::string ServerGump::virtueTooltip(uint16_t graphic, uint16_t hue) const
{
    // Hue -> rank, graphic -> virtue: PacketHandlers.CreateGump's VirtueGumpItem table.
    std::string level;

    switch (hue)
    {
    case 1154: case 1547: case 2213: case 235: case 18: case 2210: case 1348:
        level = "Seeker of ";
        break;
    case 2404: case 1552: case 2216: case 2302: case 2118: case 618: case 2212: case 1352:
        level = "Follower of ";
        break;
    case 43: case 53: case 1153: case 33: case 318: case 67: case 98:
        level = "Knight of ";
        break;
    case 2406:
        level = graphic == 0x6F ? "Seeker of " : "Knight of ";
        break;
    default:
        break;
    }

    uint32_t cliloc = 1051000;

    switch (graphic)
    {
    case 0x69: cliloc += 2; break;
    case 0x6A: cliloc += 7; break;
    case 0x6B: cliloc += 5; break;
    case 0x6D: cliloc += 6; break;
    case 0x6E: cliloc += 1; break;
    case 0x6F: cliloc += 3; break;
    case 0x70: cliloc += 4; break;
    default: break;
    }

    std::string name = context().text->cliloc(cliloc);
    return level + (name.empty() ? "Unknown virtue" : name);
}

std::vector<uint8_t> ServerGump::buildReply(uint32_t buttonId) const
{
    std::vector<uint32_t> switches;
    std::vector<std::pair<uint16_t, std::string>> entries;

    for (auto* c : controls())
    {
        if (auto* box = dynamic_cast<Checkbox*>(c); box && box->isChecked())
        {
            switches.push_back(box->localSerial());
        }
        else if (auto* entry = dynamic_cast<TextEntry*>(c))
        {
            entries.emplace_back(static_cast<uint16_t>(entry->localSerial()), entry->text());
        }
    }

    // ClassicUO replies with the gump's own id, not the master gump's.
    return uo::gumps::encodeGumpResponse(serial(), serverId(), buttonId, switches, entries);
}

void ServerGump::onButton(uint32_t buttonId)
{
    if (isClosed())
    {
        return;
    }

    if (serial() != 0 && context().send)
    {
        context().send(buildReply(buttonId));
    }

    close();
}

void ServerGump::onCloseRequested()
{
    if (!canCloseWithRightClick())
    {
        return;
    }

    onButton(0);
}

bool ServerGump::onWheel(Control* target, float dy)
{
    for (Control* c = target; c; c = dynamic_cast<Control*>(c->getParent()))
    {
        if (auto* html = dynamic_cast<HtmlArea*>(c))
        {
            html->scrollBy(dy);
            return true;
        }
    }

    return false;
}

}  // namespace uo::client::gumps
