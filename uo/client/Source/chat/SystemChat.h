// SPDX-License-Identifier: BSD-2-Clause
// Ported from ClassicUO (Game/UI/Gumps/SystemChatControl.cs).
#pragma once

#include "axmol/base/InputDelegate.h"
#include "axmol/base/KeyboardEvent.h"
#include "axmol/scene/Node.h"

#include "uo/chat/ChatLine.h"
#include "uo/chat/RecentLines.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

namespace ax
{
class Label;
class LayerColor;
}  // namespace ax

namespace uo::world
{
class World;
struct Message;
}  // namespace uo::world

// The chat line at the bottom of the game view, with the last system lines above it. The
// Axmol side of ClassicUO's SystemChatControl: uo::chat::ChatLine keeps the text and mode and
// sends the speech; this node draws them in the UO fonts (TTF when the fonts are missing) and
// takes the keyboard's text whenever no gump text entry has it.
class SystemChat : public ax::Node, public ax::InputDelegate
{
public:
    static SystemChat* create(uo::world::World& world, float width, uo::chat::ChatLine::Send send);

    uo::chat::ChatLine& line() { return *_line; }
    bool empty() const { return _line->empty(); }

    // A world message (already translated) for the lines above the chat.
    void addMessage(const uo::world::Message& msg, std::string_view text);

    // Keys the scene's input router passes on: Ctrl+Q / Ctrl+W history. Returns true when used.
    bool keyDown(ax::KeyboardEvent::KeyCode key, bool ctrl);
    // Escape: leaves a prompt. False when there was nothing to leave.
    bool escape() { return _line->escape(); }

    // Whether Ctrl is down, for Ctrl+Backspace (the IME's backspace carries no modifiers).
    std::function<bool()> ctrlHeld;

    void update(float dt) override;
    void onEnter() override;
    void onExit() override;

    bool initWith(uo::world::World& world, float width, uo::chat::ChatLine::Send send);

protected:
    // InputDelegate: typed text, Enter ("\n") and Backspace.
    bool canAttachWithIME() const override { return true; }
    bool canDetachWithIME() const override { return true; }
    void insertText(std::string_view text) override;
    void deleteBackward(unsigned int numChars) override;
    void didAttachWithIME() override;
    void didDetachWithIME() override;

private:
    void refreshLine();
    void refreshRecent();
    ax::Node* makeText(std::string_view text, std::uint8_t font, bool unicode, std::uint16_t hue, int maxWidth);

    std::unique_ptr<uo::chat::ChatLine> _line;
    uo::chat::RecentLines _recent;
    uo::world::World* _world = nullptr;

    float _width              = 0;
    ax::LayerColor* _backdrop = nullptr;
    ax::Node* _label          = nullptr;
    ax::Node* _text           = nullptr;
    ax::Node* _recentNode     = nullptr;
    std::uint32_t _shownRevision = ~0u;
    bool _recentDirty            = false;
    bool _focused                = false;
    bool _shownFocus             = false;
};
