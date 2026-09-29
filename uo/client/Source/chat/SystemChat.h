// SPDX-License-Identifier: BSD-2-Clause
// Ported from ClassicUO (Game/UI/Gumps/SystemChatControl.cs).
#pragma once

#include "axmol/base/KeyboardEvent.h"
#include "axmol/scene/Node.h"

#include "uo/chat/ChatLine.h"
#include "uo/chat/RecentLines.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <string_view>

namespace ax
{
class LayerColor;
}  // namespace ax

namespace uo::world
{
class World;
struct Message;
}  // namespace uo::world

// The chat line at the bottom of the game view, with the last system lines above it. The
// Axmol side of ClassicUO's SystemChatControl: uo::chat::ChatLine keeps the mode and sends the
// speech, a uo::client::text::TextBox (the StbTextBox port) edits the text, and this node keeps
// the two in step, draws the mode label and the recent lines, and gives the box the keyboard
// whenever no gump text entry holds it.
class SystemChat : public ax::Node
{
public:
    static SystemChat* create(uo::world::World& world, float width, uo::chat::ChatLine::Send send);

    uo::chat::ChatLine& line() { return *_line; }
    bool empty() const { return _line->empty(); }
    bool focused() const;
    // A click on the game world gives the keyboard back to the chat, as ClassicUO does when a
    // click lands on no control.
    void takeKeyboard();

    // A world message (already translated) for the lines above the chat.
    void addMessage(const uo::world::Message& msg, std::string_view text);

    // Keys the scene's input router passes on: Ctrl+Q / Ctrl+W history. True when used.
    bool keyDown(ax::KeyboardEvent::KeyCode key, bool ctrl);
    // Escape: leaves a prompt. False when there was nothing to leave.
    bool escape();

    // Whether Ctrl is down, for Ctrl+Backspace (the IME's backspace carries no modifiers).
    std::function<bool()> ctrlHeld;

    void update(float dt) override;

    bool initWith(uo::world::World& world, float width, uo::chat::ChatLine::Send send);

private:
    class Box;
    friend class Box;

    // Box edits: typed text goes to the chat line, whose prefix rules may change it back.
    void onBoxChanged();
    void onSubmit();
    void backspaceOnEmpty();
    void deleteWord();

    // Chat line state to the box and label.
    void pushLine();
    void refreshLabel();
    void refreshRecent();
    ax::Node* makeText(std::string_view text, std::uint8_t font, bool unicode, std::uint16_t hue, int maxWidth);

    std::unique_ptr<uo::chat::ChatLine> _line;
    uo::chat::RecentLines _recent;
    uo::world::World* _world = nullptr;

    float _width              = 0;
    ax::LayerColor* _backdrop = nullptr;
    ax::Node* _label          = nullptr;
    Box* _box                 = nullptr;
    ax::Node* _recentNode     = nullptr;
    std::string _shownLabel;
    std::uint32_t _shownRevision = ~0u;
    bool _syncing                = false;
    bool _recentDirty            = false;
};
