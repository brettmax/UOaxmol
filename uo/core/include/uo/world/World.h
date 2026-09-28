// SPDX-License-Identifier: BSD-2-Clause
// Ported from ClassicUO (Game/World.cs and the state-holding managers:
// PartyManager, TargetManager, ObjectPropertiesListManager, CorpseManager,
// JournalManager, ChatManager, Weather, IsometricLight).
#pragma once

#include "uo/world/BuffTable.h"
#include "uo/world/Entity.h"
#include "uo/world/Events.h"
#include "uo/world/Types.h"

#include <array>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

namespace uo::world
{

struct LightState
{
    uint8_t overall{0};
    uint8_t personal{0};
    uint8_t realOverall{0};
    uint8_t realPersonal{0};
};

struct WeatherState
{
    std::optional<WeatherType> current; // unset while no weather is active
    WeatherType type{WeatherType::Rain};
    uint8_t count{0};
    uint8_t temperature{0};

    void reset() noexcept { *this = WeatherState{}; }
};

struct MultiTarget
{
    uint16_t model{0};
    uint16_t xOffset{0};
    uint16_t yOffset{0};
    uint16_t zOffset{0};
    uint16_t hue{0};
};

struct TargetState
{
    bool isTargeting{false};
    CursorTarget state{CursorTarget::Invalid};
    TargetType type{TargetType::Neutral};
    uint32_t cursorId{0};
    std::optional<MultiTarget> multi;
    Serial lastAttack{0};
};

struct PartyState
{
    static constexpr int kSize = 10;

    Serial leader{0};
    Serial inviter{0};
    std::array<Serial, kSize> members{};

    bool contains(Serial s) const noexcept;
    void clear() noexcept { *this = PartyState{}; }
};

struct ObjectProperty
{
    uint32_t revision{0};
    std::string name;
    std::string data;
    uint32_t nameCliloc{0};
    std::vector<uint32_t> clilocs;
};

struct CorpseInfo
{
    Serial corpse{0};
    Serial owner{0};
    Direction direction{Direction::North};
    bool running{false};
};

struct SpellbookContent
{
    uint16_t graphic{0};
    uint16_t type{0};
    uint64_t spells{0}; // bit i set = spell (i + 1) known
};

// Multi footprint (from multi.mul / MultiCollection.uop) used to lay out custom house planes.
struct MultiBounds
{
    int16_t minX{0};
    int16_t minY{0};
    int16_t maxX{0};
    int16_t maxY{0};
};

struct ChatState
{
    enum class Status : uint8_t
    {
        Disabled,
        Enabled,
        EnabledUserRequest,
    };

    Status status{Status::Disabled};
    std::string currentChannel;
    std::vector<std::pair<std::string, bool>> channels; // name, hasPassword
};

class World
{
public:
    World() = default;
    World(const World&) = delete;
    World& operator=(const World&) = delete;

    // --- wiring -------------------------------------------------------------
    void setListener(WorldListener* l) noexcept { _listener = l ? l : &_nullListener; }
    void setRequests(ServerRequests* r) noexcept { _requests = r ? r : &_nullRequests; }
    void setClilocs(ClilocResolver* c) noexcept { _clilocs = c; }
    WorldListener& listener() noexcept { return *_listener; }
    ServerRequests& requests() noexcept { return *_requests; }
    ClilocResolver* clilocs() noexcept { return _clilocs; }

    // Must match the version the session logs in with (Session::Settings::version).
    ClientVersion clientVersion{makeVersion(7, 0, 15, 1)};

    // --- entities ------------------------------------------------------------
    Player* player() noexcept { return _player; }
    const Player* player() const noexcept { return _player; }
    bool isPlayer(Serial s) const noexcept { return _player && _player->serial == s; }
    // ClassicUO's InGame: a player exists and a map is loaded.
    bool inGame() const noexcept { return _player != nullptr && mapIndex >= 0; }

    Player& createPlayer(Serial serial);

    Entity* get(Serial serial) noexcept;
    Item* item(Serial serial) noexcept;
    Mobile* mobile(Serial serial) noexcept;
    Item& getOrCreateItem(Serial serial);
    Mobile& getOrCreateMobile(Serial serial);

    size_t itemCount() const noexcept { return _items.size(); }
    size_t mobileCount() const noexcept { return _mobiles.size(); }
    template <typename F> void forEachItem(F&& f)
    {
        for (auto& [s, it] : _items)
            f(*it);
    }
    template <typename F> void forEachMobile(F&& f)
    {
        for (auto& [s, m] : _mobiles)
            f(*m);
    }

    // Detaches the item from its container (if any) without destroying it.
    void removeItemFromContainer(Item& item);
    // Appends the item to container's contents; does not touch item.container.
    void pushToBack(Entity& container, Item& item);
    // Removes an item and everything inside it. Returns false when it did not exist.
    bool removeItem(Serial serial);
    // Removes a mobile and its equipment. Never removes the player.
    bool removeMobile(Serial serial);
    // Removes every child of container; with keepEquipped, children on a layer stay.
    void clearContainer(Entity& container, bool keepEquipped = false);
    // Moves a mobile to a new serial (0xAF death: the owner is re-keyed with bit 31 set).
    bool rekeyMobile(Serial from, Serial to);

    // Outermost container: the owning mobile's serial, the top item, or 0 if broken.
    Serial rootContainer(const Item& item) noexcept;
    Item* findItemByLayer(const Entity& owner, Layer layer) noexcept;
    Item* secureTradeBox(const Mobile& mobile) noexcept;

    // Clears everything (logout / new character).
    void clear();
    // Map change: drops everything not carried by the player.
    void clearForMapChange();
    void setMapIndex(int index);

    // --- world state ----------------------------------------------------------
    int mapIndex{-1};
    uint16_t rangeX{0};
    uint16_t rangeY{0};
    uint8_t clientViewRange{kMaxViewRange};
    bool skillsRequested{false};
    Season season{Season::Summer};
    Season oldSeason{Season::Summer};
    int oldMusicIndex{0};
    uint32_t lockedFeatures{0};
    uint8_t bodyConversionFlags{0};
    // Object property lists (AOS tooltips). Off for the T2A ruleset; the login flow sets it
    // from the character list flags (0xA9) when a shard enables tooltips.
    bool tooltipsEnabled{false};
    LightState light;
    WeatherState weather;
    TargetState target;
    PartyState party;
    ChatState chat;
    PromptData prompt;
    std::set<uint16_t> activeSpellIcons;
    std::vector<std::string> skillNames; // from skills.mul or 0x3A 0xFE
    std::span<const uint16_t> buffTable{defaultBuffTable()};
    std::unordered_map<Serial, SpellbookContent> spellbooks; // 0xBF 0x1B
    std::unordered_map<Serial, uint32_t> customHouseRevisions; // last decoded 0xD8 revision
    // Supplied by the client from the multi loader; needed to decode 0xD8 planes.
    std::function<std::optional<MultiBounds>(uint16_t multiGraphic)> multiBounds;
    // zlib inflate of `in` into exactly `out.size()` bytes; needed for 0xD8. uocore has no
    // zlib dependency yet, so the client supplies it.
    std::function<bool(std::span<const uint8_t> in, std::span<uint8_t> out)> inflate;

    // GameActions.RequestMobileStatus / SendCloseStatus, including HitsRequest bookkeeping.
    void requestMobileStatus(Serial serial, bool force = false);
    void closeStatus(Serial serial, bool force = false);

    void changeSeason(Season s, int music);

    // --- object property lists (tooltips) ------------------------------------
    const ObjectProperty* properties(Serial serial) const noexcept;
    void setProperties(Serial serial, ObjectProperty prop);
    bool isRevisionEqual(Serial serial, uint32_t revision) const noexcept;
    void removeProperties(Serial serial) { _opl.erase(serial); }

    // Pending 0xD6 / 0xFB requests, flushed by flushPendingRequests().
    void addMegaClilocRequest(Serial serial);
    void addCustomHouseRequest(Serial serial);
    void flushPendingRequests();
    const std::vector<Serial>& pendingClilocRequests() const noexcept { return _clilocRequests; }

    // --- corpses ---------------------------------------------------------------
    void addCorpse(Serial corpse, Serial owner, Direction dir, bool running);
    bool corpseExists(Serial corpse, Serial owner) const noexcept;
    void removeCorpse(Serial corpse, Serial owner);
    const std::deque<CorpseInfo>& corpses() const noexcept { return _corpses; }

    // --- journal -----------------------------------------------------------------
    static constexpr size_t kJournalLimit = 500;
    void addMessage(Message msg);
    const std::deque<Message>& journal() const noexcept { return _journal; }

    // --- targeting helpers -------------------------------------------------------
    void setTargeting(CursorTarget state, uint32_t cursorId, TargetType type);
    void setTargetingMulti(uint32_t deedSerial, uint16_t model, uint16_t x, uint16_t y, uint16_t z, uint16_t hue);
    void cancelTarget();

    // --- remote mobile movement --------------------------------------------------
    // Sets a mobile's position, queuing a smoothed step when possible (UpdateGameObject logic).
    void moveMobile(Mobile& m, uint16_t x, uint16_t y, int8_t z, Direction dir);

private:
    void destroyItemTree(Item& item);

    std::unordered_map<Serial, std::unique_ptr<Item>> _items;
    std::unordered_map<Serial, std::unique_ptr<Mobile>> _mobiles;
    Player* _player{nullptr};

    std::unordered_map<Serial, ObjectProperty> _opl;
    std::vector<Serial> _clilocRequests;
    std::vector<Serial> _customHouseRequests;
    std::deque<CorpseInfo> _corpses;
    std::deque<Message> _journal;

    WorldListener _nullListener;
    ServerRequests _nullRequests;
    WorldListener* _listener{&_nullListener};
    ServerRequests* _requests{&_nullRequests};
    ClilocResolver* _clilocs{nullptr};

    friend class PacketHandlers;
};

} // namespace uo::world
