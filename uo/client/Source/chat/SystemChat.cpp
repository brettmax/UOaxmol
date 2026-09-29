// SPDX-License-Identifier: BSD-2-Clause
// Ported from ClassicUO (Game/UI/Gumps/SystemChatControl.cs).

#include "SystemChat.h"

#include "axmol/TextBox.h"
#include "axmol/TextFactory.h"
#include "axmol/TextSystem.h"

#include "axmol/axmol.h"
#include "axmol/base/InputSystem.h"

#include "uo/text/JournalText.h"
#include "uo/world/Events.h"

#include <chrono>

using namespace ax;
using uo::client::text::TextBox;
using uo::client::text::TextBoxStyle;
using uo::client::text::TextLabel;
using uo::client::text::TextStyle;
using uo::client::text::TextSystem;

namespace
{

constexpr float kOffset          = 3;    // CHAT_X_OFFSET
constexpr float kRowHeight       = 15;   // CHAT_HEIGHT
constexpr float kRecentGap       = 20;   // the recent lines end this far above the text box
constexpr int kRecentWidth       = 320;  // ChatLineTime's maxWidth
constexpr std::uint8_t kChatFont = 1;    // Profile.ChatFont
constexpr int kBoxLength         = 500;  // TEXTBOX_LENGTH
constexpr const char* kTtf       = "fonts/arial.ttf";

std::uint64_t nowMs()
{
    using namespace std::chrono;
    return static_cast<std::uint64_t>(duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count());
}

}  // namespace

// The chat's TextBox, with SystemChatControl's own key rules on top of StbTextBox's:
// Backspace on an empty line drops the mode, Ctrl+Backspace deletes a word, and Escape is
// left to the scene (it cancels targets and prompts) rather than blurring the box.
class SystemChat::Box : public TextBox
{
public:
    static Box* create(SystemChat* owner, float width, float height)
    {
        auto* box = new (std::nothrow) Box(owner);
        TextBoxStyle style;
        style.font     = kChatFont;
        style.unicode  = true;
        style.border   = true;
        style.maxChars = kBoxLength;
        if (box && box->init(width, height, style))
        {
            box->autorelease();
            return box;
        }
        delete box;
        return nullptr;
    }

protected:
    explicit Box(SystemChat* owner) : _owner(owner) {}

    void deleteBackward(unsigned int numChars) override
    {
        if (_owner->ctrlHeld && _owner->ctrlHeld())
            _owner->deleteWord();
        else if (text16().empty())
            _owner->backspaceOnEmpty();
        else
            TextBox::deleteBackward(numChars);
    }

    void controlKey(KeyboardEvent::KeyCode keyCode) override
    {
        if (keyCode != KeyboardEvent::KeyCode::KEY_ESCAPE)
            TextBox::controlKey(keyCode);
    }

private:
    SystemChat* _owner;
};

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

    const float rowHeight = kRowHeight + kOffset;
    setContentSize(Size(width, rowHeight));

    // The half-transparent strip behind the text (AlphaBlendControl at 0.5).
    _backdrop = LayerColor::create(Color32(0, 0, 0, 128), width, rowHeight);
    addChild(_backdrop, 0);

    _box = Box::create(this, width - 2 * kOffset, rowHeight);
    if (!_box)
        return false;
    _box->onChanged = [this](TextBox*) { onBoxChanged(); };
    _box->onSubmit  = [this](TextBox*) { onSubmit(); };
    addChild(_box, 2);

    _recentNode = Node::create();
    addChild(_recentNode, 1);

    pushLine();
    scheduleUpdate();
    return true;
}

bool SystemChat::focused() const
{
    return _box->focused();
}

void SystemChat::takeKeyboard()
{
    _box->focus();
}

// --- box <-> chat line ---------------------------------------------------------------------

void SystemChat::onBoxChanged()
{
    if (_syncing)
        return;
    _line->setText(_box->text());
    pushLine();
}

void SystemChat::onSubmit()
{
    _line->submit();
    pushLine();
}

void SystemChat::backspaceOnEmpty()
{
    _line->backspace();
    pushLine();
}

void SystemChat::deleteWord()
{
    const int caret = _line->deleteWord(_box->caretIndex());
    pushLine();
    _box->setCaretIndex(caret);
}

void SystemChat::pushLine()
{
    _shownRevision = _line->revision();

    // A prefix that picked a mode, a sent line or a history step changes the text under the box.
    _syncing = true;
    if (_box->text() != _line->text())
        _box->setText(_line->text());
    _box->setHue(_line->hue());
    _syncing = false;

    if (_line->label() != _shownLabel)
        refreshLabel();
}

void SystemChat::refreshLabel()
{
    _shownLabel = _line->label();

    if (_label)
        _label->removeFromParent();
    _label = makeText(_shownLabel, 0, true, _line->hue(), 0);

    // The mode label sits left of the text and pushes the box right (SystemChatControl.Resize).
    float x = kOffset;
    if (_label)
    {
        _label->setPosition(Vec2(x, kRowHeight + kOffset));
        addChild(_label, 2);
        x += _label->getContentSize().width;
    }
    _box->setPositionX(x);
}

// --- input -------------------------------------------------------------------------------

bool SystemChat::keyDown(KeyboardEvent::KeyCode key, bool ctrl)
{
    if (!_box->focused() || !ctrl)
        return false;

    switch (key)
    {
    case KeyboardEvent::KeyCode::KEY_Q:
    case KeyboardEvent::KeyCode::KEY_CAPITAL_Q: _line->historyBack(); break;
    case KeyboardEvent::KeyCode::KEY_W:
    case KeyboardEvent::KeyCode::KEY_CAPITAL_W: _line->historyForward(); break;
    default: return false;
    }
    pushLine();
    return true;
}

bool SystemChat::escape()
{
    if (!_line->escape())
        return false;
    pushLine();
    return true;
}

void SystemChat::addMessage(const uo::world::Message& msg, std::string_view text)
{
    if (_recent.add(msg, text, *_world, _line->options.hues, nowMs()))
        _recentDirty = true;
}

// --- per frame -----------------------------------------------------------------------------

void SystemChat::update(float)
{
    // The chat takes the keyboard back whenever no gump text entry holds it.
    if (!InputSystem::getInstance()->hasAttachedDelegate())
        _box->focus();

    _line->syncPrompt();
    if (_line->revision() != _shownRevision)
        pushLine();

    if (_recent.expire(nowMs()))
        _recentDirty = true;
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

void SystemChat::refreshRecent()
{
    _recentDirty = false;
    _recentNode->removeAllChildren();

    // Newest at the bottom, 20 px above the text box and stacking upward
    // (SystemChatControl.AddToRenderLists); lines that would pass the top of the view are left out.
    const float limit = _director->getVisibleSize().height - getPositionY();
    float bottom      = kRowHeight + kOffset + kRecentGap;
    const auto& lines = _recent.lines();
    for (auto it = lines.rbegin(); it != lines.rend(); ++it)
    {
        auto font    = static_cast<std::uint8_t>(it->font);
        bool unicode = it->unicode;
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
