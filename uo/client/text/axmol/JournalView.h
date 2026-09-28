// SPDX-License-Identifier: BSD-2-Clause
// Ported from ClassicUO (JournalManager, JournalGump's entry list).

#pragma once

#include "axmol/TextFactory.h"

#include "axmol/scene/Node.h"

#include <cstdint>
#include <deque>
#include <string>

namespace ax
{
class ClippingRectangleNode;
}

namespace uo::client::text
{

// One journal message, already translated (cliloc and affix resolved by the world model).
struct JournalLine
{
    std::string name;
    std::string text;
    std::uint16_t hue = 0;
    bool unicode      = true;
};

// A scrollable, clipped list of journal lines, newest at the bottom. Each line renders as
// "name: text" in the entry's hue: unicode font 0 for unicode entries,
// ASCII font 9 otherwise, or unicode for everything with setForceUnicode.
//
// Feed it from the world's journal once with setEntries, then append each new message.
class JournalView : public ax::Node
{
public:
    static JournalView* create(const ax::Size& size);

    void setEntries(const std::deque<JournalLine>& entries);
    void append(const JournalLine& entry);
    void clearEntries();

    void setForceUnicode(bool value);
    void setMaxEntries(std::size_t max) { _maxEntries = max; }

    // Scrolls toward older lines by `pixels` (negative scrolls back down); clamped.
    void scrollBy(float pixels);
    void scrollToBottom();
    float scrollOffset() const { return _scroll; }

    bool initWithSize(const ax::Size& size);
    void setContentSize(const ax::Size& size) override;

private:
    TextLabel* makeLine(const JournalLine& entry) const;
    void relayout();

    ax::ClippingRectangleNode* _clip = nullptr;
    ax::Node* _content               = nullptr;
    std::deque<TextLabel*> _lines;  // children of _content, oldest first
    std::deque<JournalLine> _entries;
    std::size_t _maxEntries = 500;
    float _scroll           = 0.0f;
    bool _forceUnicode      = false;
};

}  // namespace uo::client::text
