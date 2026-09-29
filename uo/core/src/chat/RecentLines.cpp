// SPDX-License-Identifier: BSD-2-Clause
// Ported from ClassicUO (Game/UI/Gumps/SystemChatControl.cs).

#include "uo/chat/RecentLines.h"

#include "uo/world/Events.h"
#include "uo/world/World.h"

namespace uo::chat
{

namespace
{

bool isSystemName(std::string_view name)
{
    if (name.size() != 6)
    {
        return false;
    }
    constexpr std::string_view kSystem = "system";
    for (std::size_t i = 0; i < 6; ++i)
    {
        char c = name[i];
        if (c >= 'A' && c <= 'Z')
        {
            c = static_cast<char>(c - 'A' + 'a');
        }
        if (c != kSystem[i])
        {
            return false;
        }
    }
    return true;
}

}  // namespace

std::optional<RecentLine> RecentLines::fromMessage(const world::Message& msg, std::string_view text,
                                                   world::World& world, const ChatHues& hues)
{
    using world::MessageType;

    if (msg.textType == world::TextType::Client || text.empty())
    {
        return std::nullopt;
    }

    // e.Parent == null || !SerialHelper.IsValid(e.Parent.Serial)
    const bool noSpeaker = !world::isValidSerial(msg.serial) || world.get(msg.serial) == nullptr;

    RecentLine line;
    line.font    = msg.font;
    line.hue     = msg.hue;
    line.unicode = msg.unicode;

    switch (msg.type)
    {
    case MessageType::Regular:
    case MessageType::System:
        if (msg.type == MessageType::Regular && !noSpeaker)
        {
            return std::nullopt;
        }
        if (!msg.name.empty() && !isSystemName(msg.name))
        {
            line.text = msg.name + ": " + std::string(text);
        }
        else
        {
            line.text = std::string(text);
        }
        return line;

    case MessageType::Label:
        if (!noSpeaker)
        {
            return std::nullopt;
        }
        line.text = std::string(text);
        return line;

    case MessageType::Party:
        line.text = "[Party][" + msg.name + "]: " + std::string(text);
        line.hue  = hues.party;
        return line;

    case MessageType::Guild:
        line.text = "[Guild][" + msg.name + "]: " + std::string(text);
        line.hue  = hues.guild;
        return line;

    case MessageType::Alliance:
        line.text = "[Alliance][" + msg.name + "]: " + std::string(text);
        line.hue  = hues.alliance;
        return line;

    default:
        if (noSpeaker && (msg.name.empty() || isSystemName(msg.name)))
        {
            line.text = std::string(text);
            return line;
        }
        return std::nullopt;
    }
}

bool RecentLines::add(const world::Message& msg, std::string_view text, world::World& world,
                      const ChatHues& hues, std::uint64_t now)
{
    auto line = fromMessage(msg, text, world, hues);
    if (!line)
    {
        return false;
    }
    add(std::move(*line), now);
    return true;
}

void RecentLines::add(RecentLine line, std::uint64_t now)
{
    if (_lines.size() >= kMaxLines)
    {
        _lines.pop_front();
    }
    line.expires = now + kLifetimeMs;
    _lines.push_back(std::move(line));
}

bool RecentLines::expire(std::uint64_t now)
{
    bool any = false;
    while (!_lines.empty() && now > _lines.front().expires)
    {
        _lines.pop_front();
        any = true;
    }
    return any;
}

}  // namespace uo::chat
