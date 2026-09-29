// SPDX-License-Identifier: BSD-2-Clause
// Ported from ClassicUO (Game/UI/Gumps/SystemChatControl.cs, GameActions.Say/SayParty,
// MessageManager.SendServerPromptResponse): the chat line at the bottom of the game view,
// without its drawing. It keeps the typed text and the speech mode picked by its prefix
// ("; " whisper, "! " yell, ": " emote, "/" party, "\" guild, "|" alliance, "-" command),
// answers server prompts, remembers what was sent for Ctrl+Q / Ctrl+W, and sends the speech.

#pragma once

#include "uo/chat/SpeechKeywords.h"

#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace uo::world
{
class World;
}  // namespace uo::world

namespace uo::chat
{

enum class ChatMode : std::uint8_t
{
    Default,
    Whisper,
    Emote,
    Yell,
    Party,
    Guild,
    Alliance,
    ClientCommand,
    UOAMChat,
    Prompt,
    UOChat,
};

// Profile speech hues, with ClassicUO's defaults.
struct ChatHues
{
    std::uint16_t speech{0x02B2};
    std::uint16_t whisper{0x0033};
    std::uint16_t emote{0x0021};
    std::uint16_t yell{0x0021};
    std::uint16_t party{0x0044};
    std::uint16_t guild{0x0044};
    std::uint16_t alliance{0x0057};
    std::uint16_t chat{0x0256};
};

struct ChatOptions
{
    ChatHues hues;
    std::string language{"ENU"};  // Settings.GlobalSettings.Language
    std::uint8_t speechFont{3};   // GameActions.Say's font
    bool uoChatEnabled{false};    // ChatManager.ChatIsEnabled: "," opens the UO chat mode
    bool disableCtrlQW{false};    // Profile.DisableCtrlQWBtn
};

// The typed text's colour in `mode` (GetChatHue).
std::uint16_t chatHue(ChatMode mode, const ChatHues& hues);

// The label left of the text ("[Whisper]: "), empty for the default mode.
std::string_view modeLabel(ChatMode mode);

class ChatLine
{
public:
    using Send = std::function<void(std::span<const std::uint8_t>)>;

    static constexpr std::size_t kMaxMessageLength = 100;  // longer lines go out in parts
    static constexpr std::size_t kMaxTextLength    = 500;  // what the box holds

    ChatLine(world::World& world, Send send);

    ChatOptions options;
    const SpeechKeywords* keywords{nullptr};  // speech.mul, when loaded

    // "-command args": ClassicUO's CommandManager. Unhandled commands do nothing.
    std::function<void(std::string_view name, const std::vector<std::string>& args)> onClientCommand;

    const std::string& text() const noexcept { return _text; }
    ChatMode mode() const noexcept { return _mode; }
    // The mode label shown left of the text; a party tell shows "[Tell] [name]: ".
    const std::string& label() const noexcept { return _label; }
    std::uint16_t hue() const noexcept { return chatHue(_mode, options.hues); }
    bool empty() const noexcept { return _text.empty(); }

    // Bumped on every change a view has to redraw (text, mode or label).
    std::uint32_t revision() const noexcept { return _revision; }

    // Replaces the text as if typed, then applies the prefix rules (SystemChatControl.Update).
    void setText(std::string text);
    // Typed text at the end of the line; the box stops at kMaxTextLength characters.
    void insert(std::string_view utf8);
    // Backspace: removes the last character, or on an empty line returns to the default mode.
    void backspace();
    // Ctrl+Backspace: removes the last word, keeping the space before it.
    void deleteWord();
    // Ctrl+Q / Ctrl+W: step back and forward through what was sent this session.
    void historyBack();
    void historyForward();
    // Escape: leaves the prompt mode (cancelling the prompt). False when there was none.
    bool escape();
    // Enter: sends the line in its mode. Lines over 100 characters send the first part and
    // keep the rest in the box.
    void submit();

    // Follows World::prompt: a server prompt switches to the prompt mode, its end back.
    void syncPrompt();

    void setMode(ChatMode mode);

    // TrySplitMessage: the part to send now and the rest, split at the last space or newline
    // before 100 characters. False when the text fits (or the mode is single-line).
    static bool splitMessage(std::string_view text, ChatMode mode, std::string& message, std::string& remainder);

    struct HistoryEntry
    {
        ChatMode mode;
        std::string text;
    };
    const std::vector<HistoryEntry>& history() const noexcept { return _history; }

private:
    void applyPrefix();
    void showLabel(std::string label);
    void hideLabel();
    void changed() { ++_revision; }

    void send(const std::string& text, ChatMode mode);
    void say(std::string_view text, std::uint16_t hue, std::uint8_t type);
    void sayParty(std::string_view text);
    void print(std::string text, std::uint16_t hue, std::uint8_t type, bool unicode);
    void sendPromptResponse(std::string_view text);
    void reset();

    world::World& _world;
    Send _send;

    std::string _text;
    std::string _label;
    ChatMode _mode{ChatMode::Default};
    bool _promptActive{false};
    std::uint32_t _revision{0};

    std::vector<HistoryEntry> _history;
    int _historyIndex{-1};
};

}  // namespace uo::chat
