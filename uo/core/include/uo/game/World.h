// SPDX-License-Identifier: BSD-2-Clause
// Part of the AxmolUO client core. A first slice of ClassicUO.Game (World, Entity, Mobile,
// Item) and the PacketHandlers that populate it: enough to log in, see who and what is
// around, walk, and read speech. The remaining handlers port into this class.
#pragma once

#include "uo/io/ClientVersion.h"

#include <cstdint>
#include <deque>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

namespace uo::game
{

enum class Direction : std::uint8_t
{
    North = 0,
    Right = 1,  // north-east
    East  = 2,
    Down  = 3,  // south-east
    South = 4,
    Left  = 5,  // south-west
    West  = 6,
    Up    = 7,  // north-west
    Mask    = 0x07,
    Running = 0x80,
};

struct Equipment
{
    std::uint32_t serial  = 0;
    std::uint16_t graphic = 0;
    std::uint8_t layer    = 0;
    std::uint16_t hue     = 0;
};

struct Entity
{
    std::uint32_t serial  = 0;
    std::uint16_t graphic = 0;
    std::uint16_t hue     = 0;
    std::uint16_t x       = 0;
    std::uint16_t y       = 0;
    std::int8_t z         = 0;
    std::uint8_t direction = 0;
    std::uint8_t flags     = 0;
    std::string name;

    // Items
    std::uint16_t amount = 1;
    bool isMulti         = false;

    // Mobiles
    std::uint8_t notoriety = 0;
    std::uint16_t hits = 0, hitsMax = 0;
    std::uint16_t mana = 0, manaMax = 0;
    std::uint16_t stamina = 0, staminaMax = 0;
    std::vector<Equipment> equipment;

    bool isMobile() const { return (serial & 0x40000000) == 0; }
};

struct JournalEntry
{
    std::uint32_t serial = 0;  // 0xFFFFFFFF for system messages
    std::string name;
    std::string text;
    std::uint16_t hue = 0;
    std::uint8_t type = 0;
};

// Game state, mutated only by server packets and by the local walk prediction.
class World
{
public:
    explicit World(ClientVersion version = makeVersion(7, 0, 15, 1)) : _version(version) {}

    // Applies one server packet. Returns false for ids the world does not handle yet.
    bool handle(std::span<const std::uint8_t> packet);

    // --- walking ---------------------------------------------------------------------------
    // Predicts a step and returns the 0x02 request to send, or nullopt when too many steps are
    // already unacknowledged (the server allows a small window).
    std::optional<std::vector<std::uint8_t>> requestWalk(Direction dir, bool run);
    std::size_t pendingSteps() const { return _pendingSteps.size(); }

    // --- state -----------------------------------------------------------------------------
    bool inWorld() const { return _player != 0; }
    std::uint32_t playerSerial() const { return _player; }
    const Entity* player() const { return find(_player); }
    const Entity* find(std::uint32_t serial) const;
    const std::unordered_map<std::uint32_t, Entity>& entities() const { return _entities; }
    int mapIndex() const { return _map; }
    const std::deque<JournalEntry>& journal() const { return _journal; }

    // Called for every entity add/update/remove, so the scene graph can mirror the world.
    std::function<void(const Entity&)> onEntityUpdated;
    std::function<void(std::uint32_t)> onEntityRemoved;
    std::function<void(const JournalEntry&)> onMessage;
    std::function<void(int)> onMapChanged;

    static constexpr std::size_t kMaxJournal     = 500;
    static constexpr std::size_t kMaxPendingSteps = 5;

private:
    Entity& upsert(std::uint32_t serial);
    void updated(const Entity& e);
    void remove(std::uint32_t serial);
    void addMessage(JournalEntry e);

    ClientVersion _version;
    std::unordered_map<std::uint32_t, Entity> _entities;
    std::uint32_t _player = 0;
    int _map              = 0;
    std::deque<JournalEntry> _journal;

    struct Step
    {
        std::uint8_t sequence;
        std::uint16_t x, y;
        std::uint8_t direction;
    };
    std::deque<Step> _pendingSteps;
    std::uint8_t _walkSequence = 0;
};

// Tile offset for one step in `dir`.
void stepOffset(Direction dir, int& dx, int& dy);

}  // namespace uo::game
