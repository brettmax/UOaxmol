// SPDX-License-Identifier: BSD-2-Clause
#include "uo/chat/ChatLine.h"
#include "uo/chat/ChatPackets.h"
#include "uo/chat/RecentLines.h"
#include "uo/chat/SpeechKeywords.h"
#include "uo/world/Events.h"
#include "uo/world/World.h"

#include "doctest.h"

#include <string>
#include <vector>

using namespace uo::chat;
using uo::world::World;

namespace
{

using Bytes = std::vector<std::uint8_t>;

struct Rig
{
    World world{uo::makeVersion(7, 0, 15, 1)};
    std::vector<Bytes> sent;
    ChatLine chat{world, [this](std::span<const std::uint8_t> b) { sent.emplace_back(b.begin(), b.end()); }};

    Rig()
    {
        world.createPlayer(0x00000001).name = "Me";
    }

    void type(std::string_view text)
    {
        for (char c : text)
        {
            chat.insert(std::string_view(&c, 1));
        }
    }
};

Bytes utf16be(std::string_view ascii)
{
    Bytes out;
    for (char c : ascii)
    {
        out.push_back(0);
        out.push_back(static_cast<std::uint8_t>(c));
    }
    out.push_back(0);
    out.push_back(0);
    return out;
}

Bytes speechPacket(std::uint8_t type, std::uint16_t hue, std::string_view text)
{
    Bytes body{type, static_cast<std::uint8_t>(hue >> 8), static_cast<std::uint8_t>(hue), 0, 3, 'E', 'N', 'U', 0};
    const Bytes t = utf16be(text);
    body.insert(body.end(), t.begin(), t.end());
    const auto len = static_cast<std::uint16_t>(body.size() + 3);
    Bytes p{0xAD, static_cast<std::uint8_t>(len >> 8), static_cast<std::uint8_t>(len)};
    p.insert(p.end(), body.begin(), body.end());
    return p;
}

}  // namespace

TEST_CASE("chat: speech keywords match and encode like speech.mul clients")
{
    SpeechKeywords k;
    // speech.mul entries: [id BE][length BE][text].
    const Bytes file{0x00, 0x02, 0x00, 0x06, '*', 'b', 'a', 'n', 'k', '*',  //
                     0x00, 0x07, 0x00, 0x06, 'g', 'u', 'a', 'r', 'd', 's',  //
                     0x01, 0x55, 0x00, 0x05, '*', 'b', 'u', 'y', '*'};
    k.loadFromBytes(file);
    REQUIRE(k.size() == 3);

    const auto v = uo::makeVersion(7, 0, 15, 1);
    CHECK(k.match("I want my BANK box", v) == std::vector<std::uint16_t>{0x02});
    CHECK(k.match("  guards  ", v) == std::vector<std::uint16_t>{0x07});
    CHECK(k.match("call the guards", v).empty());  // no '*': the line must be the word
    CHECK(k.match("bankers", v).empty());          // not a word of its own
    CHECK(k.match("vendor buy!", v) == std::vector<std::uint16_t>{0x155});
    CHECK(k.match("bank buy", v) == std::vector<std::uint16_t>{0x02, 0x155});
    CHECK(k.match("bank", uo::makeVersion(3, 0, 5, 'c')).empty());

    CHECK(SpeechKeywords::encode(std::vector<std::uint16_t>{0x02}) == Bytes{0x00, 0x10, 0x02});
    CHECK(SpeechKeywords::encode(std::vector<std::uint16_t>{0x02, 0x155}) == Bytes{0x00, 0x20, 0x02, 0x15, 0x50});

    // Encoded speech: type | 0xC0, keyword block, UTF-8 text and a NUL.
    const std::vector<std::uint16_t> ids{0x02};
    const Bytes p = packets::unicodeSpeech("bank", 0, 3, 0x02B2, "ENU", ids);
    const Bytes expect{0xAD, 0x00, 0x14, 0xC0, 0x02, 0xB2, 0x00, 0x03, 'E', 'N',
                       'U',  0x00, 0x00, 0x10, 0x02, 'b',  'a',  'n',  'k', 0x00};
    CHECK(p.size() == 20);
    CHECK(p[1] == 0);
    CHECK(p[2] == 20);
    CHECK(std::equal(p.begin() + 3, p.end(), expect.begin() + 3));
}

TEST_CASE("chat: plain speech and the prefix modes")
{
    Rig rig;

    rig.type("hello");
    CHECK(rig.chat.mode() == ChatMode::Default);
    rig.chat.submit();
    REQUIRE(rig.sent.size() == 1);
    CHECK(rig.sent[0] == speechPacket(0, 0x02B2, "hello"));
    CHECK(rig.chat.empty());

    // "; " whispers: the prefix is eaten and the label shows.
    rig.type("; ");
    CHECK(rig.chat.mode() == ChatMode::Whisper);
    CHECK(rig.chat.label() == "[Whisper]: ");
    CHECK(rig.chat.empty());
    CHECK(rig.chat.hue() == 0x0033);
    rig.type("psst");
    rig.chat.submit();
    CHECK(rig.sent.back() == speechPacket(8, 0x0033, "psst"));
    CHECK(rig.chat.mode() == ChatMode::Default);
    CHECK(rig.chat.label().empty());

    rig.type(": ");
    rig.type("waves");
    rig.chat.submit();
    CHECK(rig.sent.back() == speechPacket(2, 0x0021, "*waves*"));

    rig.type("! help");
    rig.chat.submit();
    CHECK(rig.sent.back() == speechPacket(9, 0x0021, "help"));

    // Without the space these are ordinary speech.
    rig.type("!x");
    CHECK(rig.chat.mode() == ChatMode::Default);
    CHECK(rig.chat.text() == "!x");
    rig.chat.backspace();
    rig.chat.backspace();
    CHECK(rig.chat.empty());

    rig.type("\\");
    CHECK(rig.chat.mode() == ChatMode::Guild);
    rig.type("hi");
    rig.chat.submit();
    CHECK(rig.sent.back() == speechPacket(13, 0x0044, "hi"));

    rig.type("|");
    CHECK(rig.chat.mode() == ChatMode::Alliance);
    rig.chat.backspace();  // Backspace on an empty line drops the mode
    CHECK(rig.chat.mode() == ChatMode::Default);
    CHECK(rig.chat.label().empty());

    // "-" is a client command, "--" UOAssist.
    std::vector<std::string> command;
    rig.chat.onClientCommand = [&](std::string_view, const std::vector<std::string>& args) { command = args; };
    rig.type("-");
    CHECK(rig.chat.mode() == ChatMode::ClientCommand);
    rig.type("where  now");
    rig.chat.submit();
    CHECK(command == std::vector<std::string>{"where", "now"});
    rig.type("--");
    CHECK(rig.chat.mode() == ChatMode::UOAMChat);
    CHECK(rig.chat.label() == "[UOAM]: ");
}

TEST_CASE("chat: long lines go out in parts, history steps back")
{
    Rig rig;
    std::string word(9, 'a');  // "aaaaaaaaa " x 25 = 250 characters
    std::string text;
    for (int i = 0; i < 25; ++i)
    {
        text += word + " ";
    }

    std::string message, remainder;
    REQUIRE(ChatLine::splitMessage(text, ChatMode::Default, message, remainder));
    CHECK(message.size() == 99);  // the space at index 99 is the last break at or before 100
    CHECK(remainder.size() == 150);
    CHECK_FALSE(ChatLine::splitMessage(text, ChatMode::Prompt, message, remainder));

    rig.chat.setText(text);
    rig.chat.submit();
    CHECK(rig.sent.size() == 1);
    CHECK(rig.chat.text().size() == 150);
    rig.chat.submit();
    rig.chat.submit();
    CHECK(rig.sent.size() == 3);
    CHECK(rig.chat.empty());

    // Ctrl+Q walks back through what was sent, Ctrl+W forward and then to an empty line.
    rig.type("; ");
    rig.type("one");
    rig.chat.submit();
    rig.chat.historyBack();
    CHECK(rig.chat.mode() == ChatMode::Whisper);
    CHECK(rig.chat.text() == "one");
    rig.chat.historyBack();
    CHECK(rig.chat.mode() == ChatMode::Default);
    CHECK(rig.chat.text().size() == 50);  // the third part of the long line
    rig.chat.historyForward();
    CHECK(rig.chat.text() == "one");
    rig.chat.historyForward();
    CHECK(rig.chat.empty());

    // Ctrl+Backspace removes the last word and keeps the space before it.
    rig.chat.setText("go to the bank");
    rig.chat.deleteWord();
    CHECK(rig.chat.text() == "go to the ");
    rig.chat.deleteWord();
    CHECK(rig.chat.text() == "go to ");
}

TEST_CASE("chat: party lines and commands")
{
    Rig rig;

    // Not in a party: a note to self, nothing sent.
    rig.type("/");
    CHECK(rig.chat.mode() == ChatMode::Party);
    rig.type("hello");
    rig.chat.submit();
    CHECK(rig.sent.empty());
    REQUIRE_FALSE(rig.world.journal().empty());
    CHECK(rig.world.journal().back().text == "Note to self: hello");

    rig.type("/");
    rig.type("add");
    rig.chat.submit();
    REQUIRE(rig.sent.size() == 1);
    CHECK(rig.sent[0] == Bytes{0xBF, 0x00, 0x0A, 0x00, 0x06, 0x01, 0, 0, 0, 0});

    // In a party, "/3 text" tells member 3.
    rig.world.party.leader     = 0x00000001;
    rig.world.party.members[0] = 0x00000001;
    rig.world.party.members[2] = 0x00000033;
    rig.world.getOrCreateMobile(0x00000033).name = "Ally";
    rig.chat.setText("/3 hi");
    CHECK(rig.chat.mode() == ChatMode::Party);
    CHECK(rig.chat.label() == "[Tell] [Ally]: ");
    CHECK(rig.chat.text() == "3 ");
    rig.type("there");
    rig.chat.submit();
    const Bytes& tell = rig.sent.back();
    CHECK(tell[5] == 0x03);
    CHECK((tell[6] == 0 && tell[7] == 0 && tell[8] == 0 && tell[9] == 0x33));

    rig.type("/");
    rig.type("all");
    rig.chat.submit();
    CHECK(rig.sent.back()[5] == 0x04);

    rig.type("/");
    rig.type("quit");
    rig.chat.submit();
    CHECK(rig.sent.back() == Bytes{0xBF, 0x00, 0x0A, 0x00, 0x06, 0x02, 0, 0, 0, 1});
}

TEST_CASE("chat: server prompts take the next line")
{
    Rig rig;
    rig.world.prompt = {uo::world::PromptKind::ASCII, 0x0102030405060708ull};
    rig.chat.syncPrompt();
    CHECK(rig.chat.mode() == ChatMode::Prompt);
    CHECK(rig.chat.label() == "[Prompt]:");

    rig.type("Rune");
    rig.chat.submit();
    REQUIRE(rig.sent.size() == 1);
    CHECK(rig.sent[0] == Bytes{0x9A, 0x00, 0x14, 1, 2, 3, 4, 5, 6, 7, 8, 0, 0, 0, 1, 'R', 'u', 'n', 'e', 0});
    CHECK(rig.world.prompt.kind == uo::world::PromptKind::None);
    CHECK(rig.chat.mode() == ChatMode::Default);

    // Escape cancels a unicode prompt.
    rig.world.prompt = {uo::world::PromptKind::Unicode, 9};
    rig.chat.syncPrompt();
    CHECK(rig.chat.escape());
    REQUIRE(rig.sent.size() == 2);
    CHECK(rig.sent[1] ==
          Bytes{0xC2, 0x00, 0x13, 0, 0, 0, 0, 0, 0, 0, 9, 0, 0, 0, 0, 'E', 'N', 'U', 0});
    CHECK_FALSE(rig.chat.escape());
}

TEST_CASE("chat: old clients speak ASCII")
{
    World world{uo::makeVersion(1, 26, 4, 0)};
    std::vector<Bytes> sent;
    ChatLine chat{world, [&](std::span<const std::uint8_t> b) { sent.emplace_back(b.begin(), b.end()); }};
    chat.setText("hi");
    chat.submit();
    REQUIRE(sent.size() == 1);
    CHECK(sent[0] == Bytes{0x03, 0x00, 0x0B, 0x00, 0x02, 0xB2, 0x00, 0x03, 'h', 'i', 0});
}

TEST_CASE("chat: recent lines show speaker-less messages for 10 seconds")
{
    World world{uo::makeVersion(7, 0, 15, 1)};
    world.getOrCreateMobile(0x00000050).name = "Bob";
    ChatHues hues;
    RecentLines lines;

    uo::world::Message system;
    system.name = "System";
    system.text = "Welcome";
    system.type = uo::world::MessageType::Regular;
    CHECK(lines.add(system, system.text, world, hues, 1000));

    uo::world::Message speech;
    speech.serial = 0x00000050;
    speech.name   = "Bob";
    speech.text   = "hail";
    CHECK_FALSE(lines.add(speech, speech.text, world, hues, 1000));  // shown over Bob instead

    uo::world::Message guild = speech;
    guild.type               = uo::world::MessageType::Guild;
    CHECK(lines.add(guild, guild.text, world, hues, 2000));
    CHECK(lines.lines().back().text == "[Guild][Bob]: hail");
    CHECK(lines.lines().back().hue == hues.guild);

    uo::world::Message client = system;
    client.textType           = uo::world::TextType::Client;
    CHECK_FALSE(lines.add(client, client.text, world, hues, 2000));

    CHECK_FALSE(lines.expire(11000));
    CHECK(lines.expire(11001));
    CHECK(lines.lines().size() == 1);

    for (int i = 0; i < 40; ++i)
    {
        lines.add(RecentLine{"x", 0, 0, false, 0}, 5000);
    }
    CHECK(lines.lines().size() == RecentLines::kMaxLines);
}
