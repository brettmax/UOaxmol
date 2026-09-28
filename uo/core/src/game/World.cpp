// SPDX-License-Identifier: BSD-2-Clause
#include "uo/game/World.h"

#include "uo/io/BinaryReader.h"
#include "uo/net/OutgoingPackets.h"

namespace uo::game
{

void stepOffset(Direction dir, int& dx, int& dy)
{
    static constexpr int kDx[8] = {0, 1, 1, 1, 0, -1, -1, -1};
    static constexpr int kDy[8] = {-1, -1, 0, 1, 1, 1, 0, -1};
    int d                       = static_cast<int>(dir) & 7;
    dx                          = kDx[d];
    dy                          = kDy[d];
}

const Entity* World::find(std::uint32_t serial) const
{
    auto it = _entities.find(serial);
    return it == _entities.end() ? nullptr : &it->second;
}

Entity& World::upsert(std::uint32_t serial)
{
    Entity& e = _entities[serial];
    e.serial  = serial;
    return e;
}

void World::updated(const Entity& e)
{
    if (onEntityUpdated)
        onEntityUpdated(e);
}

void World::remove(std::uint32_t serial)
{
    if (_entities.erase(serial) && onEntityRemoved)
        onEntityRemoved(serial);
}

void World::addMessage(JournalEntry e)
{
    _journal.push_back(std::move(e));
    if (_journal.size() > kMaxJournal)
        _journal.pop_front();
    if (onMessage)
        onMessage(_journal.back());
}

std::optional<std::vector<std::uint8_t>> World::requestWalk(Direction dir, bool run)
{
    auto it = _entities.find(_player);
    if (it == _entities.end() || _pendingSteps.size() >= kMaxPendingSteps)
        return std::nullopt;

    Entity& p             = it->second;
    std::uint8_t d        = static_cast<std::uint8_t>(dir) & 7;
    std::uint8_t sequence = _walkSequence;

    // Turning in place is a step of its own in UO; only a step in the facing direction moves.
    if ((p.direction & 7) == d)
    {
        int dx, dy;
        stepOffset(dir, dx, dy);
        p.x = static_cast<std::uint16_t>(p.x + dx);
        p.y = static_cast<std::uint16_t>(p.y + dy);
    }
    p.direction = d;

    _pendingSteps.push_back({sequence, p.x, p.y, d});
    // Sequence 0 is only valid for the first step after login or a resync.
    _walkSequence = _walkSequence == 255 ? 1 : static_cast<std::uint8_t>(_walkSequence + 1);
    updated(p);

    return net::out::walkRequest(static_cast<std::uint8_t>(d | (run ? 0x80 : 0)), sequence);
}

bool World::handle(std::span<const std::uint8_t> packet)
{
    io::BinaryReader r(packet);
    std::uint8_t id = r.readU8();

    switch (id)
    {
    case 0x1B:  // enter world
    {
        std::uint32_t serial = r.readU32BE();
        r.skip(4);
        Entity& e    = upsert(serial);
        e.graphic    = r.readU16BE();
        e.x          = r.readU16BE();
        e.y          = r.readU16BE();
        e.z          = static_cast<std::int8_t>(r.readU16BE());
        e.direction  = r.readU8() & 7;
        _player      = serial;
        _walkSequence = 0;
        _pendingSteps.clear();
        updated(e);
        return true;
    }

    case 0x20:  // update player (teleport, graphic or hue change)
    {
        std::uint32_t serial = r.readU32BE();
        Entity& e            = upsert(serial);
        e.graphic            = static_cast<std::uint16_t>(r.readU16BE() + r.readU8());
        e.hue                = r.readU16BE();
        e.flags              = r.readU8();
        e.x                  = r.readU16BE();
        e.y                  = r.readU16BE();
        r.readU16BE();  // server id
        e.direction = r.readU8() & 7;
        e.z         = r.readI8();
        if (serial == _player)
        {
            _pendingSteps.clear();
            _walkSequence = 0;
        }
        updated(e);
        return true;
    }

    case 0x77:  // mobile moving
    case 0x78:  // mobile incoming (with equipment)
    {
        if (id == 0x78)
            r.skip(2);  // length
        std::uint32_t serial = r.readU32BE();
        Entity& e            = upsert(serial);
        e.graphic            = r.readU16BE();
        e.x                  = r.readU16BE();
        e.y                  = r.readU16BE();
        e.z         = r.readI8();
        e.direction = r.readU8();
        e.hue       = r.readU16BE();
        e.flags     = r.readU8();
        e.notoriety = r.readU8();

        if (id == 0x78)
        {
            e.equipment.clear();
            while (r.remaining() >= 4)
            {
                std::uint32_t itemSerial = r.readU32BE();
                if (itemSerial == 0)
                    break;
                Equipment eq;
                eq.serial  = itemSerial;
                eq.graphic = r.readU16BE();
                eq.layer   = r.readU8();
                if (_version >= cv::CV_70331 || (eq.graphic & 0x8000))
                    eq.hue = r.readU16BE();
                eq.graphic &= 0x7FFF;
                e.equipment.push_back(eq);
            }
        }
        updated(e);
        return true;
    }

    case 0x1A:  // world item (classic)
    {
        r.skip(2);  // length
        std::uint32_t serial  = r.readU32BE();
        std::uint16_t graphic = r.readU16BE();
        std::uint16_t amount  = 1;
        if (serial & 0x80000000)
            amount = r.readU16BE();
        if (graphic & 0x8000)
            graphic = static_cast<std::uint16_t>((graphic & 0x7FFF) + r.readU8());
        std::uint16_t x = r.readU16BE();
        std::uint16_t y = r.readU16BE();
        std::uint8_t direction = 0;
        if (x & 0x8000)
            direction = r.readU8();
        std::int8_t z = r.readI8();
        std::uint16_t hue = 0;
        std::uint8_t flags = 0;
        if (y & 0x8000)
            hue = r.readU16BE();
        if (y & 0x4000)
            flags = r.readU8();

        Entity& e    = upsert(serial & 0x7FFFFFFF);
        e.graphic    = graphic;
        e.amount     = amount;
        e.x          = x & 0x7FFF;
        e.y          = y & 0x3FFF;
        e.z          = z;
        e.direction  = direction;
        e.hue        = hue;
        e.flags      = flags;
        e.isMulti    = graphic >= 0x4000;
        updated(e);
        return true;
    }

    case 0xF3:  // world item (Stygian Abyss+)
    {
        r.skip(2);
        std::uint8_t type    = r.readU8();
        std::uint32_t serial = r.readU32BE();
        Entity& e            = upsert(serial);
        e.graphic            = r.readU16BE();
        e.direction          = r.readU8();
        e.amount             = r.readU16BE();
        r.readU16BE();
        e.x         = r.readU16BE();
        e.y         = r.readU16BE();
        e.z         = r.readI8();
        r.readU8();  // light / facing
        e.hue       = r.readU16BE();
        e.flags     = r.readU8();
        e.isMulti   = type == 2;
        updated(e);
        return true;
    }

    case 0x1D:  // delete
        remove(r.readU32BE());
        return true;

    case 0x22:  // walk confirmed
    {
        std::uint8_t seq = r.readU8();
        while (!_pendingSteps.empty())
        {
            bool match = _pendingSteps.front().sequence == seq;
            _pendingSteps.pop_front();
            if (match)
                break;
        }
        if (auto it = _entities.find(_player); it != _entities.end())
            it->second.notoriety = r.readU8() & ~0x40;
        return true;
    }

    case 0x21:  // walk denied: snap back to where the server says we are
    {
        r.readU8();  // sequence
        if (auto it = _entities.find(_player); it != _entities.end())
        {
            Entity& p   = it->second;
            p.x         = r.readU16BE();
            p.y         = r.readU16BE();
            p.direction = r.readU8();
            p.z         = r.readI8();
            updated(p);
        }
        _pendingSteps.clear();
        _walkSequence = 0;
        return true;
    }

    case 0x1C:  // ASCII message
    {
        r.skip(2);
        JournalEntry j;
        j.serial = r.readU32BE();
        r.readU16BE();  // graphic
        j.type = r.readU8();
        j.hue  = r.readU16BE();
        r.readU16BE();  // font
        j.name = r.readASCII(30);
        j.text = r.readASCII();
        addMessage(std::move(j));
        return true;
    }

    case 0xAE:  // unicode message
    {
        r.skip(2);
        JournalEntry j;
        j.serial = r.readU32BE();
        r.readU16BE();
        j.type = r.readU8();
        j.hue  = r.readU16BE();
        r.readU16BE();
        r.skip(4);  // language
        j.name = r.readASCII(30);
        j.text = r.readUnicodeBE();
        addMessage(std::move(j));
        return true;
    }

    case 0x11:  // status bar
    {
        r.skip(2);
        Entity& e = upsert(r.readU32BE());
        e.name    = r.readASCII(30);
        e.hits    = r.readU16BE();
        e.hitsMax = r.readU16BE();
        updated(e);
        return true;
    }

    case 0xA1:  // hits
    case 0xA2:  // mana
    case 0xA3:  // stamina
    {
        Entity& e         = upsert(r.readU32BE());
        std::uint16_t max = r.readU16BE();
        std::uint16_t cur = r.readU16BE();
        if (id == 0xA1)
            e.hitsMax = max, e.hits = cur;
        else if (id == 0xA2)
            e.manaMax = max, e.mana = cur;
        else
            e.staminaMax = max, e.stamina = cur;
        updated(e);
        return true;
    }

    case 0xBF:  // general info; 0x08 = set map
    {
        r.skip(2);
        if (r.readU16BE() == 0x08)
        {
            _map = r.readU8();
            if (onMapChanged)
                onMapChanged(_map);
        }
        return true;
    }

    default:
        return false;
    }
}

}  // namespace uo::game
