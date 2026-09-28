// SPDX-License-Identifier: BSD-2-Clause
// Ported from ClassicUO (MessageManager.CreateMessage, TextContainer, TextRenderer and
// GameObject/Mobile.UpdateTextCoordsV).

#pragma once

#include "axmol/TextFactory.h"

#include "uo/text/MessageTypes.h"
#include "uo/text/SpeechText.h"

#include "axmol/scene/Node.h"

#include <cstdint>
#include <deque>
#include <functional>
#include <optional>
#include <string_view>
#include <unordered_map>

namespace uo::client::text
{

// Speech and labels above mobiles and items. Each owner keeps its five newest messages,
// stacked upward from its head with the newest lowest; text stays up for the speech delay,
// fades out over its last second, and older text that overlaps newer text goes
// half-transparent, as in the original client.
//
// Coordinates are UO screen pixels (origin top-left, y down) inside the game viewport. The
// layer should cover the viewport, with its origin at the viewport's bottom-left corner.
class OverheadTextLayer : public ax::Node
{
public:
    static constexpr int kMaxMessagesPerOwner = 5;

    // Where an owner's text stacks from: the point above its head, already offset the way
    // UpdateTextCoordsV does (art/animation height, mount, health bar). nullopt hides it.
    using AnchorProvider = std::function<std::optional<ax::Vec2>(std::uint32_t serial)>;

    static OverheadTextLayer* create();

    void setAnchorProvider(AnchorProvider provider) { _anchor = std::move(provider); }
    // The game viewport in UO screen pixels; text is kept inside it.
    void setViewport(const ax::Rect& viewport);
    // Height of the system chat box at the viewport's bottom, which text stays above.
    void setBottomInset(float inset) { _bottomInset = inset; }
    void setSpeechDelay(const uo::text::SpeechDelaySettings& settings) { _delay = settings; }
    void setTextFading(bool fading) { _textFading = fading; }

    // Adds a message over `serial` (MessageManager.CreateMessage + GameObject.AddMessage).
    void addMessage(std::uint32_t serial, std::string_view utf8, std::uint16_t hue, std::uint8_t font, bool unicode,
                    uo::text::MessageType type = uo::text::MessageType::Regular,
                    uo::text::TextType textType = uo::text::TextType::Object);

    void removeOwner(std::uint32_t serial);
    void clear();

    // The owner of the topmost message under a UO screen point, if any.
    std::optional<std::uint32_t> hitTest(const ax::Vec2& uoPoint) const;

    void update(float delta) override;
    bool init() override;

private:
    struct Message
    {
        TextLabel* label = nullptr;  // retained as a child
        std::uint32_t owner = 0;
        std::uint64_t order = 0;
        std::int64_t expires = 0;
        uo::text::MessageType type = uo::text::MessageType::Regular;
        ax::Vec2 position;  // UO screen, top-left of the text
        std::uint8_t alpha = 0xFF;
        bool transparent   = false;
        bool placed        = false;
    };

    static std::int64_t nowMs();
    void destroy(Message& message);
    void layoutOwner(std::uint32_t owner, std::deque<Message>& stack, std::int64_t now);
    void place(Message& message);

    std::unordered_map<std::uint32_t, std::deque<Message>> _stacks;
    AnchorProvider _anchor;
    ax::Rect _viewport;
    float _bottomInset = 0.0f;
    uo::text::SpeechDelaySettings _delay;
    bool _textFading    = true;
    std::uint64_t _next = 0;
};

}  // namespace uo::client::text
