// SPDX-License-Identifier: BSD-2-Clause
// Ported from ClassicUO (SystemChatControl.ChatOnMessageReceived, AddLine, ChatLineTime): the
// system lines stacked above the chat line. Only messages with no speaker in the world show
// there (speech shows over heads instead); each stays 10 seconds, 30 at most.

#pragma once

#include "uo/chat/ChatLine.h"

#include <cstdint>
#include <deque>
#include <optional>
#include <string>
#include <string_view>

namespace uo::world
{
class World;
struct Message;
}  // namespace uo::world

namespace uo::chat
{

struct RecentLine
{
    std::string text;
    std::uint16_t font{0};
    std::uint16_t hue{0};
    bool unicode{false};
    std::uint64_t expires{0};  // ms
};

class RecentLines
{
public:
    static constexpr std::size_t kMaxLines      = 30;
    static constexpr std::uint64_t kLifetimeMs  = 10000;  // TIME_DISPLAY_SYSTEM_MESSAGE_TEXT

    // The line a message adds, if any. `text` is the message's resolved text (cliloc applied).
    static std::optional<RecentLine> fromMessage(const world::Message& msg, std::string_view text,
                                                 world::World& world, const ChatHues& hues);

    // Adds the message's line, if it has one. True when something was added.
    bool add(const world::Message& msg, std::string_view text, world::World& world, const ChatHues& hues,
             std::uint64_t now);
    void add(RecentLine line, std::uint64_t now);

    // Drops expired lines. True when any went.
    bool expire(std::uint64_t now);

    const std::deque<RecentLine>& lines() const noexcept { return _lines; }  // oldest first
    void clear() { _lines.clear(); }

private:
    std::deque<RecentLine> _lines;
};

}  // namespace uo::chat
