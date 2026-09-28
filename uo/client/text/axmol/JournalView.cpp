// SPDX-License-Identifier: BSD-2-Clause
// Ported from ClassicUO (JournalManager, JournalGump's entry list).

#include "axmol/JournalView.h"

#include "axmol/TextSystem.h"

#include "uo/text/JournalText.h"

#include "axmol/2d/ClippingRectangleNode.h"

#include <algorithm>

namespace uo::client::text
{

namespace
{
constexpr float kLineGap = 2.0f;
}

JournalView* JournalView::create(const ax::Size& size)
{
    auto* view = new JournalView();

    if (view->initWithSize(size))
    {
        view->autorelease();
        return view;
    }

    delete view;
    return nullptr;
}

bool JournalView::initWithSize(const ax::Size& size)
{
    if (!ax::Node::init())
        return false;

    _clip = ax::ClippingRectangleNode::create();
    addChild(_clip);
    _content = ax::Node::create();
    _clip->addChild(_content);
    setContentSize(size);
    return true;
}

void JournalView::setContentSize(const ax::Size& size)
{
    ax::Node::setContentSize(size);

    if (_clip)
    {
        _clip->setClippingRegion(ax::Rect(ax::Vec2::zero, size));

        // Rewrap every line at the new width.
        const auto entries = _entries;
        setEntries(entries);
    }
}

TextLabel* JournalView::makeLine(const JournalLine& entry) const
{
    auto& system = TextSystem::instance();

    if (!system.ready())
        return nullptr;

    const auto font = uo::text::journalFont(entry.unicode, _forceUnicode);

    TextStyle style;
    style.font     = font.font;
    style.unicode  = font.unicode;
    style.hue      = entry.hue;
    style.maxWidth = std::max(1, static_cast<int>(getContentSize().width) - 4);

    return TextLabel::create(uo::text::journalLine(entry.name, entry.text), style);
}

void JournalView::setEntries(const std::deque<JournalLine>& entries)
{
    clearEntries();

    for (const auto& e : entries)
        append(e);
}

void JournalView::append(const JournalLine& entry)
{
    _entries.push_back(entry);

    if (TextLabel* line = makeLine(entry))
    {
        _content->addChild(line);
        _lines.push_back(line);
    }
    else
    {
        _lines.push_back(nullptr);
    }

    while (_entries.size() > _maxEntries)
    {
        if (_lines.front())
            _lines.front()->removeFromParent();
        _lines.pop_front();
        _entries.pop_front();
    }

    relayout();
}

void JournalView::clearEntries()
{
    for (TextLabel* line : _lines)
    {
        if (line)
            line->removeFromParent();
    }

    _lines.clear();
    _entries.clear();
    _scroll = 0.0f;
    relayout();
}

void JournalView::setForceUnicode(bool value)
{
    if (_forceUnicode == value)
        return;

    _forceUnicode      = value;
    const auto entries = _entries;
    setEntries(entries);
}

void JournalView::scrollBy(float pixels)
{
    _scroll += pixels;
    relayout();
}

void JournalView::scrollToBottom()
{
    _scroll = 0.0f;
    relayout();
}

void JournalView::relayout()
{
    if (!_content)
        return;

    // Lines stack upward from the bottom edge, newest lowest.
    float y     = 0.0f;
    float total = 0.0f;

    for (auto it = _lines.rbegin(); it != _lines.rend(); ++it)
    {
        if (!*it)
            continue;

        const float h = (*it)->getContentSize().height;
        (*it)->setPosition(0.0f, y + h);  // labels anchor at their top-left
        y += h + kLineGap;
        total = y;
    }

    const float maxScroll = std::max(0.0f, total - getContentSize().height);
    _scroll               = std::clamp(_scroll, 0.0f, maxScroll);
    _content->setPosition(0.0f, -_scroll);
}

}  // namespace uo::client::text
