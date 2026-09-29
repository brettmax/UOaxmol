// SPDX-License-Identifier: BSD-2-Clause
// Ported from ClassicUO (Game/UI/Gumps/SystemChatControl.cs).

#include "SystemChat.h"

#include "axmol/TextFactory.h"
#include "axmol/TextSystem.h"

#include "axmol/axmol.h"
#include "axmol/base/InputSystem.h"

#include "uo/text/JournalText.h"
#include "uo/world/Events.h"

#include <chrono>

using namespace ax;
using uo::client::text::TextLabel;
using uo::client::text::TextStyle;
using uo::client::text::TextSystem;

namespace
{

constexpr float kOffset     = 3;   // CHAT_X_OFFSET
constexpr float kRowHeight  = 15;  // CHAT_HEIGHT
constexpr float kRecentGap  = 20;  // the recent lines end this far above the text box
constexpr int kRecentWidth  = 320; // ChatLineTime's maxWidth
constexpr std::uint8_t kChatFont = 1;  // Profile.ChatFont
constexpr const char* kTtf       = "fonts/arial.ttf";

std::uint64_t nowMs()
{
    using namespace std::chrono;
    return static_cast<std::uint64_t>(duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count());
}

}  // namespace

SystemChat* SystemChat::create(uo::world::World& world, float width, uo::chat::ChatLine::Send send)
{
    auto* node = new (std::nothrow) SystemChat();
    if (node && node->initWith(world, width, std::move(send)))
    {
        node->autorelease();
        return node;
    }
    delete node;
    return nullptr;
}

bool SystemChat::initWith(uo::world::World& world, float width, uo::chat::ChatLine::Send send)
{
    if (!Node::init())
        return false;

    _world = &world;
    _width = width;
    _line  = std::make_unique<uo::chat::ChatLine>(world, std::move(send));

    setContentSize(Size(width, kRowHeight + kOffset));

    // The half-transparent strip behind the text (AlphaBlendControl at 0.5).
    _backdrop = LayerColor::create(Color32(0, 0, 0, 128), width, kRowHeight + kOffset);
    addChild(_backdrop, 0);

    _recentNode = Node::create();
    addChild(_recentNode, 1);

    scheduleUpdate();
    return true;
}

void SystemChat::onEnter()
{
    Node::onEnter();
    attachWithIME();
}

void SystemChat::onExit()
{
    detachWithIME();
    Node::onExit();
}

// --- input -------------------------------------------------------------------------------

void SystemChat::didAttachWithIME()
{
    _focused = true;
}

void SystemChat::didDetachWithIME()
{
    _focused = false;
}

void SystemChat::insertText(std::string_view text)
{
    // Enter arrives as "\n": everything before it is typed, then the line is sent.
    while (!text.empty())
    {
        const std::size_t nl = text.find_first_of("\r\n");
        _line->insert(text.substr(0, nl));
        if (nl == std::string_view::npos)
            break;
        if (text[nl] == '\n')
            _line->submit();
        text.remove_prefix(nl + 1);
    }
}

void SystemChat::deleteBackward(unsigned int numChars)
{
    if (ctrlHeld && ctrlHeld())
    {
        _line->deleteWord();
        return;
    }
    for (unsigned int i = 0; i < std::max(1u, numChars); ++i)
        _line->backspace();
}

bool SystemChat::keyDown(KeyboardEvent::KeyCode key, bool ctrl)
{
    if (!_focused || !ctrl)
        return false;

    switch (key)
    {
    case KeyboardEvent::KeyCode::KEY_Q: _line->historyBack(); return true;
    case KeyboardEvent::KeyCode::KEY_W: _line->historyForward(); return true;
    default: return false;
    }
}

void SystemChat::addMessage(const uo::world::Message& msg, std::string_view text)
{
    if (_recent.add(msg, text, *_world, _line->options.hues, nowMs()))
        _recentDirty = true;
}

// --- drawing -----------------------------------------------------------------------------

void SystemChat::update(float)
{
    // The chat takes the keyboard back whenever no gump text entry holds it.
    if (!InputSystem::getInstance()->hasAttachedDelegate())
        attachWithIME();

    _line->syncPrompt();

    if (_recent.expire(nowMs()))
        _recentDirty = true;

    if (_line->revision() != _shownRevision || _focused != _shownFocus)
        refreshLine();
    if (_recentDirty)
        refreshRecent();
}

Node* SystemChat::makeText(std::string_view text, std::uint8_t font, bool unicode, std::uint16_t hue, int maxWidth)
{
    if (text.empty())
        return nullptr;

    if (TextSystem::instance().ready())
    {
        TextStyle style;
        style.font     = font;
        style.unicode  = unicode;
        style.hue      = hue;
        style.maxWidth = maxWidth;
        style.border   = true;  // FontStyle.BlackBorder
        return TextLabel::create(text, style);
    }

    // No UO fonts: a plain outlined label.
    auto* label = Label::createWithTTF(std::string(text), kTtf, 14);
    if (!label)
        return nullptr;
    label->setAnchorPoint(Vec2(0, 1));
    if (maxWidth > 0)
        label->setDimensions(static_cast<float>(maxWidth), 0);
    label->enableOutline(Color32::black, 1);
    return label;
}

void SystemChat::refreshLine()
{
    _shownRevision = _line->revision();
    _shownFocus    = _focused;

    if (_label)
        _label->removeFromParent();
    if (_text)
        _text->removeFromParent();
    _label = _text = nullptr;

    const std::uint16_t hue = _line->hue();
    const float top         = kRowHeight + kOffset;
    float x                 = kOffset;

    // The mode label sits left of the text in the mode's hue (unicode font 0).
    if ((_label = makeText(_line->label(), 0, true, hue, 0)))
    {
        _label->setPosition(Vec2(x, top));
        addChild(_label, 2);
        x += _label->getContentSize().width;
    }

    // The text in the chat font, with the caret while the chat has the keyboard.
    std::string shown = _line->text();
    if (_focused)
        shown += '_';
    if ((_text = makeText(shown, kChatFont, true, hue, 0)))
    {
        // Past the right edge the start of the line scrolls out of view.
        const float overflow = std::max(0.f, x + _text->getContentSize().width - (_width - kOffset));
        _text->setPosition(Vec2(x - overflow, top));
        addChild(_text, 2);
    }
}

void SystemChat::refreshRecent()
{
    _recentDirty = false;
    _recentNode->removeAllChildren();

    // Newest at the bottom, each 20 px above the text box and stacking upward
    // (SystemChatControl.AddToRenderLists); lines that reach the top of the view are left out.
    const float limit = _director->getVisibleSize().height - getPositionY();
    float bottom      = kRowHeight + kOffset + kRecentGap;
    const auto& lines = _recent.lines();
    for (auto it = lines.rbegin(); it != lines.rend(); ++it)
    {
        std::uint8_t font = static_cast<std::uint8_t>(it->font);
        bool unicode      = it->unicode;
        if (TextSystem::instance().ready())
        {
            const auto f = uo::text::speechFont(it->font, it->unicode, TextSystem::instance().fonts());
            font         = f.font;
            unicode      = f.unicode;
        }

        Node* label = makeText(it->text, font, unicode, it->hue, kRecentWidth);
        if (!label)
            continue;
        const float height = label->getContentSize().height;
        if (bottom + height > limit)
            break;
        label->setPosition(Vec2(2, bottom + height));
        _recentNode->addChild(label);
        bottom += height;
    }
}
