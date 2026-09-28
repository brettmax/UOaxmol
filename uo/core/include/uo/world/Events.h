// SPDX-License-Identifier: BSD-2-Clause
//
// Boundaries between the world model and the rest of the client.
//
// ClassicUO's handlers reach straight into UIManager, the audio player and the
// socket. Here those side effects are split out into three narrow interfaces so
// uo/core stays engine-free:
//
//   WorldListener  - notifications for the Axmol client (gumps, audio, overhead text,
//                    effects). Every method has an empty default.
//   ServerRequests - packets the handlers need to send back to the server. The
//                    network layer implements it with the outgoing packet builders.
//   ClilocResolver - cliloc lookup/translation, implemented over the cliloc loader.
#pragma once

#include "uo/world/Entity.h"
#include "uo/world/Types.h"

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace uo::world
{

class Entity;
class Item;
class Mobile;
class Player;
class World;

struct Message
{
    Serial serial{0};         // 0 / 0xFFFFFFFF for system messages
    uint16_t graphic{0};
    MessageType type{MessageType::Regular};
    uint16_t hue{0};
    uint16_t font{0};
    std::string name;
    std::string text;
    std::string lang;         // unicode messages only
    TextType textType{TextType::System};
    bool unicode{false};
    uint32_t cliloc{0};       // 0xC1/0xCC/0xBF 0x10 source cliloc, when any
    // 0xC1/0xCC: raw arguments (tab-separated) and affix. Without a ClilocResolver `text` stays
    // empty and the text layer resolves cliloc + clilocArgs + affix itself.
    std::string clilocArgs;
    std::string affix;        // 0xCC only
    bool affixPrepend{false}; // 0xCC only
};

struct Effect
{
    GraphicEffectType type{GraphicEffectType::Moving};
    Serial source{0};
    Serial target{0};
    uint16_t graphic{0};
    uint16_t hue{0};
    uint16_t sourceX{0};
    uint16_t sourceY{0};
    int8_t sourceZ{0};
    uint16_t targetX{0};
    uint16_t targetY{0};
    int8_t targetZ{0};
    uint8_t speed{0};
    int duration{0};
    bool fixedDirection{false};
    bool explode{false};
    GraphicEffectBlendMode blendMode{GraphicEffectBlendMode::Normal};
    // 0xC7 particle extension
    uint16_t tileId{0};
    uint16_t explodeEffect{0};
    uint16_t explodeSound{0};
    Serial effectSerial{0};
    uint8_t layer{0};
};

struct SoundRequest
{
    uint8_t mode{0};
    uint16_t index{0};
    uint16_t volume{0};
    uint16_t x{0};
    uint16_t y{0};
    int16_t z{0};
};

struct ShopEntry
{
    Serial serial{0};
    uint16_t graphic{0};
    uint16_t hue{0};
    uint16_t amount{0};
    uint32_t price{0};
    std::string name;
    bool nameIsCliloc{false};
    uint32_t cliloc{0}; // set when the server sent a cliloc number as the name
};

struct ShopData
{
    Serial vendor{0};
    bool isBuy{true};
    std::vector<ShopEntry> entries;
};

struct TradeEvent
{
    enum class Kind : uint8_t
    {
        Open = 0,
        Close = 1,
        AcceptState = 2,
        TheirGold = 3,
        MyGold = 4,
    };

    Kind kind{Kind::Open};
    Serial container{0};
    Serial myBox{0};
    Serial theirBox{0};
    std::string theirName;
    bool iAccept{false};
    bool theyAccept{false};
    uint32_t gold{0};
    uint32_t platinum{0};
};

struct BookHeader
{
    Serial serial{0};
    bool editable{false};
    bool isNewPacket{false}; // 0xD4
    uint16_t pageCount{0};
    std::string title;
    std::string author;
};

struct BookPage
{
    int page{0}; // zero-based
    std::vector<std::string> lines;
};

struct BookContent
{
    Serial serial{0};
    std::vector<BookPage> pages;
};

struct BulletinEvent
{
    enum class Kind : uint8_t
    {
        Open = 0,
        Summary = 1,
        Message = 2,
    };

    Kind kind{Kind::Open};
    Serial board{0};
    Serial message{0};
    Serial parent{0};
    std::string boardName; // Open
    std::string poster;
    std::string subject;
    std::string time;
    std::string body; // Message: lines joined with '\n'
};

struct MenuEntry
{
    uint16_t graphic{0};
    uint16_t hue{0};
    std::string text;
};

struct MenuData
{
    Serial serial{0};
    uint16_t menuId{0};
    std::string title;
    bool isGray{false}; // question menu (no item graphics)
    std::vector<MenuEntry> entries;
};

struct TextEntryDialog
{
    Serial serial{0};
    uint8_t parentId{0};
    uint8_t buttonId{0};
    std::string text;
    bool canCancel{false};
    uint8_t variant{0};
    uint32_t maxLength{0};
    std::string description;
};

enum class PromptKind : uint8_t
{
    None,
    ASCII,
    Unicode,
};

struct PromptData
{
    PromptKind kind{PromptKind::None};
    uint64_t data{0};
};

struct PopupMenuEntry
{
    uint32_t cliloc{0};
    uint16_t index{0};
    uint16_t hue{0xFFFF};
    uint16_t replacedHue{0};
    uint16_t flags{0};
};

struct PopupMenu
{
    Serial serial{0};
    std::vector<PopupMenuEntry> entries;
};

struct MapDisplay
{
    Serial serial{0};
    uint16_t gumpId{0};
    uint16_t startX{0};
    uint16_t startY{0};
    uint16_t endX{0};
    uint16_t endY{0};
    uint16_t width{0};
    uint16_t height{0};
    std::optional<uint16_t> facet; // 0xF5 or 3.0.8z+ clients
};

struct MapPinMessage
{
    Serial serial{0};
    uint8_t command{0}; // MapMessageType: 1 add, 2 insert, 3 move, 4 remove, 5 clear, 6 edit, 7 edit response
    uint8_t plotState{0};
    uint16_t x{0};
    uint16_t y{0};
};

enum class CloseUiKind : uint32_t
{
    Paperdoll = 1,
    Status = 2,
    Profile = 8,
    Container = 0x0C,
};

struct CustomHouseComponent
{
    uint16_t graphic{0};
    int8_t offsetX{0};
    int8_t offsetY{0};
    int8_t offsetZ{0};
};

struct CustomHouseData
{
    Serial serial{0};
    uint32_t revision{0};
    std::vector<CustomHouseComponent> components; // offsets relative to the foundation
};

struct ChatEvent
{
    uint16_t command{0};
    std::string channel;
    std::string user;
    std::string text;
    bool hasPassword{false};
};

struct WorldMapEntityUpdate
{
    Serial serial{0};
    uint16_t x{0};
    uint16_t y{0};
    uint8_t map{0};
    int hits{0};
    bool isGuild{false};
};

struct Waypoint
{
    Serial serial{0};
    uint16_t x{0};
    uint16_t y{0};
    int8_t z{0};
    uint8_t map{0};
    uint16_t type{0};
    bool ignoreObject{false};
    uint32_t cliloc{0};
    std::string name;
};

class WorldListener
{
public:
    virtual ~WorldListener() = default;

    // Lifecycle
    virtual void onEnterWorld(Player&) {}
    virtual void onLoginComplete(Player&) {}
    virtual void onLogoutResponse(bool /*allowed*/) {}
    virtual void onMapChanged(int /*mapIndex*/) {}
    virtual void onMapPatches(std::span<const uint8_t> /*raw 0xBF 0x18 payload*/) {}
    virtual void onPing(uint8_t /*sequence*/) {}

    // Entities
    virtual void onEntityCreated(Entity&) {}
    virtual void onEntityUpdated(Entity&) {}
    virtual void onEntityRemoved(Serial, EntityKind) {}
    virtual void onPlayerTeleported(Player&) {} // 0x20 / 0xF3 self: walker must resync
    virtual void onContainerContentsChanged(Serial /*container*/) {}
    virtual void onEquipmentChanged(Serial /*mobile*/) {}
    virtual void onNameChanged(Entity&) {}
    virtual void onStatsChanged(Entity&) {}
    virtual void onPlayerWeaponChanged(Player&) {} // re-resolve weapon abilities
    virtual void onAnimationRequested(Mobile&) {}
    virtual void onMobileDied(Mobile& /*owner (already re-keyed)*/, Serial /*corpse*/, bool /*running*/) {}
    virtual void onSkillsChanged(int /*skillIndex, -1 for all*/, bool /*openWindow*/) {}
    virtual void onSkillListChanged() {}
    virtual void onBuffsChanged() {}

    // Speech and text
    virtual void onMessage(const Message&) {}
    virtual void onDamage(Serial, uint16_t /*amount*/) {}
    virtual void onPromptChanged(const PromptData&) {}
    virtual void onChat(const ChatEvent&) {}

    // Effects and environment
    virtual void onEffect(const Effect&) {}
    virtual void onSound(const SoundRequest&) {}
    virtual void onMusic(int /*index, -1 to stop*/) {}
    virtual void onLightChanged() {}
    virtual void onWeatherChanged() {}
    virtual void onSeasonChanged(Season, int /*music*/) {}
    virtual void onDeathScreen() {}

    // Targeting
    virtual void onTargetCursorChanged() {}

    // Windows the Axmol UI opens
    virtual void onOpenContainer(Item&, uint16_t /*gumpGraphic*/) {}
    virtual void onOpenSpellbook(Item&) {}
    virtual void onSpellbookContentsChanged(Item&) {}
    virtual void onOpenPaperdoll(Mobile&, const std::string& /*title*/, bool /*canLift*/) {}
    virtual void onOpenShop(const ShopData&) {}
    virtual void onCloseShop(Serial /*vendor*/) {}
    virtual void onTrade(const TradeEvent&) {}
    virtual void onBookOpen(const BookHeader&) {}
    virtual void onBookContent(const BookContent&) {}
    virtual void onBulletinBoard(const BulletinEvent&) {}
    virtual void onMenu(const MenuData&) {}
    virtual void onTextEntryDialog(const TextEntryDialog&) {}
    virtual void onTip(uint32_t /*tip*/, uint8_t /*flag*/, const std::string& /*text*/) {}
    virtual void onOpenUrl(const std::string&) {}
    virtual void onProfile(Serial, const std::string& /*header*/, const std::string& /*footer*/, const std::string& /*body*/) {}
    virtual void onQuestArrow(Serial, bool /*display*/, uint16_t /*x*/, uint16_t /*y*/) {}
    virtual void onContextMenu(const PopupMenu&) {}
    virtual void onCloseServerGump(Serial /*gumpSerial*/, uint32_t /*button*/) {}
    virtual void onCloseUi(CloseUiKind, Serial) {}
    virtual void onDyeWindow(Serial, uint16_t /*graphic*/) {}
    virtual void onMapDisplay(const MapDisplay&) {}
    virtual void onMapPin(const MapPinMessage&) {}
    virtual void onRaceChangeWindow(bool /*female*/, uint8_t /*race*/) {}
    virtual void onSpellIconState(uint16_t /*spell*/, bool /*active*/) {}
    virtual void onPartyChanged() {}
    virtual void onPartyInvite(Serial /*inviter*/) {}
    virtual void onHouseRevision(Serial, uint32_t /*revision*/) {}
    virtual void onCustomHouse(const CustomHouseData&) {}
    virtual void onHouseCustomization(Serial, uint8_t /*type: 4 begin, 5 end*/) {}
    virtual void onWorldMapEntities(const std::vector<WorldMapEntityUpdate>&) {}
    virtual void onWaypoint(const Waypoint&) {}
    virtual void onWaypointRemoved(Serial) {}
    virtual void onBoatMoved(Item& /*multi*/, uint8_t /*speed*/, Direction /*moving*/, Direction /*facing*/) {}

    // Drag and drop (the cursor's held item lives in the client)
    virtual void onDropRejected(uint8_t /*code*/) {}
    virtual void onDragEnded() {}
    virtual void onDropAccepted() {}
};

class ServerRequests
{
public:
    virtual ~ServerRequests() = default;

    virtual void clientVersion() {}
    virtual void requestSkills(Serial) {}
    virtual void requestMobileStatus(Serial) {}
    virtual void closeStatus(Serial) {}
    virtual void singleClick(Serial) {}
    virtual void doubleClick(Serial) {}
    virtual void ackTalk() {}
    virtual void ackUnicodeSystemTalk() {}
    virtual void megaClilocRequest(const std::vector<Serial>&) {}
    virtual void customHouseDataRequest(Serial) {}
    virtual void bookPageRequest(Serial, uint16_t /*page*/) {}
    virtual void bulletinSummaryRequest(Serial /*board*/, Serial /*message*/) {}
    virtual void chatJoin(const std::string& /*channel*/) {}
    virtual void openChat(const std::string& /*name*/) {}
    virtual void clientType() {}
    virtual void clientViewRange(uint8_t) {}
    virtual void razorAck() {}
};

class ClilocResolver
{
public:
    virtual ~ClilocResolver() = default;

    // Plain string for a cliloc id; empty when unknown.
    virtual std::string get(uint32_t cliloc) = 0;
    // Substitutes tab-separated arguments into ~1_NAME~ placeholders.
    // Returns std::nullopt when the cliloc is unknown.
    virtual std::optional<std::string> translate(uint32_t cliloc, std::string_view args, bool capitalize) = 0;
};

} // namespace uo::world
