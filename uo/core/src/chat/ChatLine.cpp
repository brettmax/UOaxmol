// SPDX-License-Identifier: BSD-2-Clause
// Ported from ClassicUO (Game/UI/Gumps/SystemChatControl.cs, GameActions.Say/SayParty/Print,
// MessageManager.SendServerPromptResponse).

#include "uo/chat/ChatLine.h"

#include "uo/chat/ChatPackets.h"
#include "uo/text/Utf.h"
#include "uo/world/Types.h"
#include "uo/world/World.h"

#include <algorithm>
#include <charconv>

namespace uo::chat
{

namespace
{

using world::MessageType;

constexpr std::uint16_t kSystemHue = 0xFFFF;

std::string_view trimStart(std::string_view s)
{
    while (!s.empty() && (s.front() == ' ' || s.front() == '\n' || s.front() == '\t' || s.front() == '\r'))
    {
        s.remove_prefix(1);
    }
    return s;
}

// int.TryParse over `s` allowing surrounding spaces.
bool parseInt(std::string_view s, int& out)
{
    while (!s.empty() && s.front() == ' ')
    {
        s.remove_prefix(1);
    }
    while (!s.empty() && s.back() == ' ')
    {
        s.remove_suffix(1);
    }
    if (s.empty())
    {
        return false;
    }
    const auto [end, ec] = std::from_chars(s.data(), s.data() + s.size(), out);
    return ec == std::errc{} && end == s.data() + s.size();
}

std::string lowerAscii(std::string_view s)
{
    std::string out(s);
    for (char& c : out)
    {
        if (c >= 'A' && c <= 'Z')
        {
            c = static_cast<char>(c - 'A' + 'a');
        }
    }
    return out;
}

// Removes the last UTF-8 code point.
void popCodePoint(std::string& s)
{
    while (!s.empty())
    {
        const auto c = static_cast<unsigned char>(s.back());
        s.pop_back();
        if ((c & 0xC0) != 0x80)
        {
            break;
        }
    }
}

}  // namespace

std::uint16_t chatHue(ChatMode mode, const ChatHues& hues)
{
    switch (mode)
    {
    case ChatMode::Default: return hues.speech;
    case ChatMode::Whisper: return hues.whisper;
    case ChatMode::Emote: return hues.emote;
    case ChatMode::Yell: return hues.yell;
    case ChatMode::Party: return hues.party;
    case ChatMode::Guild: return hues.guild;
    case ChatMode::Alliance: return hues.alliance;
    case ChatMode::ClientCommand: return 1161;
    case ChatMode::UOAMChat: return 83;
    case ChatMode::UOChat: return hues.chat;
    case ChatMode::Prompt: return 946;
    }
    return 33;
}

std::string_view modeLabel(ChatMode mode)
{
    switch (mode)
    {
    case ChatMode::Default: return {};
    case ChatMode::Whisper: return "[Whisper]: ";
    case ChatMode::Emote: return "[Emote]: ";
    case ChatMode::Yell: return "[Yell]: ";
    case ChatMode::Party: return "[Party]: ";
    case ChatMode::Guild: return "[Guild]: ";
    case ChatMode::Alliance: return "[Alliance]: ";
    case ChatMode::ClientCommand: return "[Command]: ";
    case ChatMode::UOAMChat: return "[UOAM]: ";
    case ChatMode::UOChat: return "Chat";
    case ChatMode::Prompt: return "[Prompt]:";
    }
    return {};
}

ChatLine::ChatLine(world::World& world, Send send) : _world(world), _send(std::move(send))
{
    if (!_send)
    {
        _send = [](std::span<const std::uint8_t>) {};
    }
}

// --- mode and label ----------------------------------------------------------------------

void ChatLine::showLabel(std::string label)
{
    // AppendChatModePrefix: only the first label of a mode change sticks, and it empties the box.
    if (!_label.empty())
    {
        return;
    }
    _label = std::move(label);
    _text.clear();
    changed();
}

void ChatLine::hideLabel()
{
    if (!_label.empty())
    {
        _label.clear();
        changed();
    }
}

void ChatLine::setMode(ChatMode mode)
{
    const ChatMode previous = _mode;
    _mode                   = mode;

    if (previous == ChatMode::Prompt && mode != ChatMode::Prompt)
    {
        sendPromptResponse({});  // CancelServerPrompt
    }

    switch (mode)
    {
    case ChatMode::Default:
        hideLabel();
        _text.clear();
        break;
    case ChatMode::UOAMChat:
    case ChatMode::UOChat:
        hideLabel();
        showLabel(std::string(modeLabel(mode)));
        break;
    default: showLabel(std::string(modeLabel(mode))); break;
    }
    changed();
}

void ChatLine::applyPrefix()
{
    if (_mode == ChatMode::Default && !_text.empty())
    {
        const auto second = [this](char c) { return _text.size() > 1 && _text[1] == c; };

        switch (_text[0])
        {
        case '/':
        {
            // "/3 text" tells party member 3.
            std::size_t pos = 1;
            while (pos < _text.size() && _text[pos] != ' ')
            {
                ++pos;
            }
            int index = 0;
            if (pos < _text.size() && parseInt(std::string_view(_text).substr(1, pos), index) && index > 0 &&
                index < 11)
            {
                const world::Serial member = _world.party.members[index - 1];
                const world::Entity* e     = member != 0 ? _world.get(member) : nullptr;
                if (member != 0)
                {
                    showLabel("[Tell] [" + (e != nullptr ? e->name : std::string()) + "]: ");
                }
                else
                {
                    showLabel("[Tell] []: ");
                }
                setMode(ChatMode::Party);
                _text = std::to_string(index) + " ";
                changed();
            }
            else
            {
                setMode(ChatMode::Party);
            }
            break;
        }
        case '\\': setMode(ChatMode::Guild); break;
        case '|': setMode(ChatMode::Alliance); break;
        case '-': setMode(ChatMode::ClientCommand); break;
        case ',':
            if (options.uoChatEnabled)
            {
                setMode(ChatMode::UOChat);
            }
            break;
        case ':':
            if (second(' '))
            {
                setMode(ChatMode::Emote);
            }
            break;
        case ';':
            if (second(' '))
            {
                setMode(ChatMode::Whisper);
            }
            break;
        case '!':
            if (second(' '))
            {
                setMode(ChatMode::Yell);
            }
            break;
        default: break;
        }
    }
    else if (_mode == ChatMode::ClientCommand && _text == "-")
    {
        setMode(ChatMode::UOAMChat);
    }
}

// --- editing -----------------------------------------------------------------------------

void ChatLine::setText(std::string text)
{
    _text = std::move(text);
    changed();
    applyPrefix();
}

void ChatLine::insert(std::string_view utf8)
{
    const std::size_t used = uo::text::utf8ToUtf16(_text).size();
    if (used >= kMaxTextLength)
    {
        return;
    }
    std::u16string add = uo::text::utf8ToUtf16(utf8);
    add.erase(std::remove_if(add.begin(), add.end(), [](char16_t c) { return c == u'\r' || c == u'\n'; }), add.end());
    if (add.empty())
    {
        return;
    }
    add.resize(std::min(add.size(), kMaxTextLength - used));
    setText(_text + uo::text::utf16ToUtf8(add));
}

void ChatLine::backspace()
{
    if (_text.empty())
    {
        setMode(ChatMode::Default);
        return;
    }
    popCodePoint(_text);
    changed();
}

int ChatLine::deleteWord(int caret)
{
    if (_text.empty())
    {
        setMode(ChatMode::Default);
        return 0;
    }

    std::u16string wide = uo::text::utf8ToUtf16(_text);
    const int size      = static_cast<int>(wide.size());
    if (caret < 0 || caret > size)
    {
        caret = size;
    }

    // The character before the caret is skipped, so pressing again after a space keeps
    // going. (ClassicUO means to, but its LastIndexOf(' ', caret - 1) finds that space.)
    int index = -1;
    for (int i = caret - 2; i >= 0; --i)
    {
        if (wide[static_cast<std::size_t>(i)] == u' ')
        {
            index = i;
            break;
        }
    }

    int newCaret = 0;
    if (index >= 0)
    {
        wide     = wide.substr(0, static_cast<std::size_t>(index + 1)) + wide.substr(static_cast<std::size_t>(caret));
        newCaret = index + 1;
    }
    else
    {
        wide.clear();
    }
    _text = uo::text::utf16ToUtf8(wide);
    changed();

    if (_text.empty())
    {
        setMode(ChatMode::Default);
    }
    return newCaret;
}

void ChatLine::historyBack()
{
    if (options.disableCtrlQW || _historyIndex < 0 || _history.empty())
    {
        return;
    }
    if (_historyIndex > 0)
    {
        --_historyIndex;
    }
    const HistoryEntry entry = _history[static_cast<std::size_t>(_historyIndex)];
    setMode(entry.mode);
    _text = entry.text;
    changed();
}

void ChatLine::historyForward()
{
    if (options.disableCtrlQW)
    {
        return;
    }
    if (_historyIndex < static_cast<int>(_history.size()) - 1)
    {
        ++_historyIndex;
        const HistoryEntry entry = _history[static_cast<std::size_t>(_historyIndex)];
        setMode(entry.mode);
        _text = entry.text;
    }
    else
    {
        _text.clear();
    }
    changed();
}

bool ChatLine::escape()
{
    if (_mode != ChatMode::Prompt)
    {
        return false;
    }
    setMode(ChatMode::Default);
    return true;
}

void ChatLine::syncPrompt()
{
    const bool active = _world.prompt.kind != world::PromptKind::None;
    if (active == _promptActive)
    {
        return;
    }
    _promptActive = active;

    if (!active && _mode == ChatMode::Prompt)
    {
        setMode(ChatMode::Default);
    }
    else if (active && _mode != ChatMode::Prompt)
    {
        setMode(ChatMode::Prompt);
    }
}

// --- sending -----------------------------------------------------------------------------

bool ChatLine::splitMessage(std::string_view text, ChatMode mode, std::string& message, std::string& remainder)
{
    const std::u16string wide = uo::text::utf8ToUtf16(text);
    if (wide.size() <= kMaxMessageLength || mode == ChatMode::ClientCommand || mode == ChatMode::Prompt)
    {
        message = std::string(text);
        remainder.clear();
        return false;
    }

    // LastIndexOfAny([' ', '\n'], 100): the last break at or before index 100.
    std::size_t split = kMaxMessageLength;
    for (std::size_t i = kMaxMessageLength + 1; i-- > 0;)
    {
        if (wide[i] == u' ' || wide[i] == u'\n')
        {
            split = i;
            break;
        }
    }

    message   = uo::text::utf16ToUtf8(wide.substr(0, split));
    remainder = std::string(trimStart(uo::text::utf16ToUtf8(wide.substr(split))));
    return true;
}

void ChatLine::reset()
{
    _text.clear();
    setMode(ChatMode::Default);
    hideLabel();
}

void ChatLine::submit()
{
    const std::string text = _text;

    // ClassicUO sends an empty line too; servers ignore it, so it is only a reset here.
    if (text.empty())
    {
        reset();
        return;
    }

    std::string message;
    std::string remainder;
    if (splitMessage(text, _mode, message, remainder))
    {
        const ChatMode mode = _mode;
        send(message, mode);

        // Keep the party member being told, or the rest would go to the whole party.
        int member = 0;
        if (mode == ChatMode::Party && parseInt(std::string_view(text).substr(0, 2), member) && member > 0 &&
            member < 11)
        {
            remainder = std::to_string(member) + " " + remainder;
        }
        _text = std::move(remainder);
        changed();
    }
    else
    {
        send(text, _mode);
        _text.clear();
        changed();
    }

    if (_text.empty())
    {
        reset();
    }
}

void ChatLine::send(const std::string& text, ChatMode mode)
{
    _history.push_back({_mode, text});
    _historyIndex = static_cast<int>(_history.size());

    const ChatHues& hues = options.hues;
    switch (mode)
    {
    case ChatMode::Default: say(text, hues.speech, static_cast<std::uint8_t>(MessageType::Regular)); break;
    case ChatMode::Whisper: say(text, hues.whisper, static_cast<std::uint8_t>(MessageType::Whisper)); break;
    case ChatMode::Emote: say("*" + text + "*", hues.emote, static_cast<std::uint8_t>(MessageType::Emote)); break;
    case ChatMode::Yell: say(text, hues.yell, static_cast<std::uint8_t>(MessageType::Yell)); break;
    case ChatMode::Prompt: sendPromptResponse(text); break;
    case ChatMode::Party: sayParty(text); break;
    case ChatMode::Guild: say(text, hues.guild, static_cast<std::uint8_t>(MessageType::Guild)); break;
    case ChatMode::Alliance: say(text, hues.alliance, static_cast<std::uint8_t>(MessageType::Alliance)); break;
    case ChatMode::ClientCommand:
    {
        std::vector<std::string> args;
        std::size_t start = 0;
        while (start < text.size())
        {
            const std::size_t end = std::min(text.find(' ', start), text.size());
            if (end > start)
            {
                args.emplace_back(text.substr(start, end - start));
            }
            start = end + 1;
        }
        if (!args.empty() && onClientCommand)
        {
            onClientCommand(args[0], args);
        }
        break;
    }
    case ChatMode::UOAMChat:  // UOAssist is not part of this client
    case ChatMode::UOChat:    // nor is the 0xB3 chat system
        break;
    }
}

void ChatLine::say(std::string_view text, std::uint16_t hue, std::uint8_t type)
{
    if (!_send)
    {
        return;
    }

    const ClientVersion version          = _world.clientVersion;
    const std::vector<std::uint16_t> ids = keywords != nullptr ? keywords->match(text, version)
                                                               : std::vector<std::uint16_t>{};
    if (version >= cv::CV_200)
    {
        _send(packets::unicodeSpeech(text, type, options.speechFont, hue, options.language, ids));
    }
    else
    {
        _send(packets::asciiSpeech(text, type, options.speechFont, hue, !ids.empty()));
    }
}

void ChatLine::sayParty(std::string_view text)
{
    world::PartyState& party     = _world.party;
    const world::Player* player  = _world.player();
    const world::Serial self     = player != nullptr ? player->serial : 0;
    const std::string lowered    = lowerAscii(text);
    const auto notLeader         = [this] { print("You are not party leader.", kSystemHue, 0, false); };
    const auto notInParty        = [this] { print("You are not in a party.", kSystemHue, 0, false); };
    const auto noInvite          = [this] { print("No one has invited you to be in a party.", kSystemHue, 0, false); };

    if (lowered == "add")
    {
        if (party.leader == 0 || party.leader == self)
        {
            _send(packets::partyInvite());
        }
        else
        {
            notLeader();
        }
    }
    else if (lowered == "loot")
    {
        // ClassicUO flips PartyManager.CanLoot, which nothing sends to the server.
        if (party.leader == 0)
        {
            notInParty();
        }
    }
    else if (lowered == "quit")
    {
        if (party.leader == 0)
        {
            notInParty();
        }
        else
        {
            _send(packets::partyRemove(self));
        }
    }
    else if (lowered == "accept")
    {
        if (party.leader == 0 && party.inviter != 0)
        {
            _send(packets::partyAccept(party.inviter));
            party.leader  = party.inviter;
            party.inviter = 0;
        }
        else
        {
            noInvite();
        }
    }
    else if (lowered == "decline")
    {
        if (party.leader == 0 && party.inviter != 0)
        {
            _send(packets::partyDecline(party.inviter));
            party.leader  = 0;
            party.inviter = 0;
        }
        else
        {
            noInvite();
        }
    }
    else if (lowered == "rem")
    {
        if (party.leader != 0 && party.leader == self)
        {
            _send(packets::partyRemove(0));
        }
        else
        {
            notLeader();
        }
    }
    else if (party.leader != 0)
    {
        // "3 text" goes to member 3 only; the number stays in the text, as ClassicUO sends it.
        world::Serial to = 0;
        const std::size_t pos = std::min(text.find(' '), text.size());
        int index             = 0;
        if (pos < text.size() && parseInt(text.substr(0, pos), index) && index > 0 && index < 11 &&
            party.members[index - 1] != 0)
        {
            to = party.members[index - 1];
        }
        _send(packets::partyMessage(text, to));
    }
    else
    {
        print("Note to self: " + std::string(text), 0, static_cast<std::uint8_t>(MessageType::System), false);
    }
}

void ChatLine::print(std::string text, std::uint16_t hue, std::uint8_t type, bool unicode)
{
    // MessageManager.HandleMessage with no parent: a system line in the journal.
    world::Message msg;
    msg.serial   = 0;
    msg.type     = static_cast<MessageType>(type);
    msg.hue      = hue;
    msg.font     = 3;
    msg.name     = "System";
    msg.text     = std::move(text);
    msg.textType = world::TextType::System;
    msg.unicode  = unicode;
    _world.addMessage(std::move(msg));
}

void ChatLine::sendPromptResponse(std::string_view text)
{
    const world::PromptData prompt = _world.prompt;
    if (_send)
    {
        if (prompt.kind == world::PromptKind::ASCII)
        {
            _send(packets::asciiPromptResponse(prompt.data, text));
        }
        else if (prompt.kind == world::PromptKind::Unicode)
        {
            _send(packets::unicodePromptResponse(prompt.data, text, options.language));
        }
    }
    _world.prompt  = {};
    _promptActive  = false;
}

}  // namespace uo::chat
