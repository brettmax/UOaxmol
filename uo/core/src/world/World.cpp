// SPDX-License-Identifier: BSD-2-Clause
// Ported from ClassicUO (Game/World.cs and state managers).
#include "uo/world/World.h"

#include <algorithm>

namespace uo::world
{

bool PartyState::contains(Serial s) const noexcept
{
    return s != 0 && std::find(members.begin(), members.end(), s) != members.end();
}

Player& World::createPlayer(Serial serial)
{
    if (_player)
        clear();

    auto p = std::make_unique<Player>(serial);
    _player = p.get();
    p->skills.resize(skillNames.size());
    for (size_t i = 0; i < skillNames.size(); ++i)
        p->skills[i].name = skillNames[i];
    _mobiles[serial] = std::move(p);
    return *_player;
}

Entity* World::get(Serial serial) noexcept
{
    if (isMobileSerial(serial))
    {
        if (auto* m = mobile(serial))
            return m;
        return item(serial);
    }
    if (auto* i = item(serial))
        return i;
    return mobile(serial);
}

Item* World::item(Serial serial) noexcept
{
    auto it = _items.find(serial);
    return it == _items.end() ? nullptr : it->second.get();
}

Mobile* World::mobile(Serial serial) noexcept
{
    auto it = _mobiles.find(serial);
    return it == _mobiles.end() ? nullptr : it->second.get();
}

Item& World::getOrCreateItem(Serial serial)
{
    auto& slot = _items[serial];
    if (!slot)
        slot = std::make_unique<Item>(serial);
    return *slot;
}

Mobile& World::getOrCreateMobile(Serial serial)
{
    auto& slot = _mobiles[serial];
    if (!slot)
        slot = std::make_unique<Mobile>(serial);
    return *slot;
}

void World::removeItemFromContainer(Item& item)
{
    const Serial cont = item.container;

    // ClassicUO checks against 0xFFFFFFFF rather than IsValid, because a dying
    // mobile's items point at the re-keyed (bit 31) owner.
    if (cont != kInvalidSerial)
    {
        if (Entity* c = get(cont))
        {
            std::erase(c->_contents, item.serial);
            if (c->isMobile())
                _listener->onEquipmentChanged(cont);
            else
                _listener->onContainerContentsChanged(cont);
        }
        item.container = kInvalidSerial;
    }
}

void World::pushToBack(Entity& container, Item& item)
{
    auto& v = container._contents;
    std::erase(v, item.serial);
    v.push_back(item.serial);
}

void World::destroyItemTree(Item& item)
{
    // Copy: children detach themselves from item._contents as they go.
    const std::vector<Serial> children = item._contents;
    for (Serial child : children)
        removeItem(child);
}

bool World::removeItem(Serial serial)
{
    auto it = _items.find(serial);
    if (it == _items.end())
        return false;

    Item& item = *it->second;
    removeItemFromContainer(item);
    destroyItemTree(item);
    _opl.erase(serial);

    _items.erase(serial);
    _listener->onEntityRemoved(serial, EntityKind::Item);
    return true;
}

bool World::removeMobile(Serial serial)
{
    auto it = _mobiles.find(serial);
    if (it == _mobiles.end() || it->second.get() == _player)
        return false;

    Mobile& m = *it->second;
    const std::vector<Serial> children = m._contents;
    for (Serial child : children)
        removeItem(child);

    _opl.erase(serial);
    // Mirrors Entity.Destroy: tell the server we no longer track this status bar.
    closeStatus(serial, m.hitsRequest >= HitsRequest::Pending);

    _mobiles.erase(serial);
    _listener->onEntityRemoved(serial, EntityKind::Mobile);
    return true;
}

void World::clearContainer(Entity& container, bool keepEquipped)
{
    const std::vector<Serial> children = container._contents;
    for (Serial child : children)
    {
        Item* it = item(child);
        if (!it)
        {
            std::erase(container._contents, child);
            continue;
        }
        if (keepEquipped && it->layer != Layer::Invalid)
            continue;
        removeItem(child);
    }
}

bool World::rekeyMobile(Serial from, Serial to)
{
    auto it = _mobiles.find(from);
    if (it == _mobiles.end() || from == to)
        return false;

    std::unique_ptr<Mobile> m = std::move(it->second);
    _mobiles.erase(it);
    m->serial = to;
    for (Serial child : m->_contents)
    {
        if (Item* i = item(child))
            i->container = to;
    }
    _mobiles[to] = std::move(m);
    return true;
}

Serial World::rootContainer(const Item& start) noexcept
{
    const Item* it = &start;
    while (isItemSerial(it->container))
    {
        it = item(it->container);
        if (!it)
            return 0;
    }
    return isMobileSerial(it->container) ? it->container : it->serial;
}

Item* World::findItemByLayer(const Entity& owner, Layer layer) noexcept
{
    for (Serial s : owner._contents)
    {
        Item* it = item(s);
        if (it && it->layer == layer)
            return it;
    }
    return nullptr;
}

Item* World::secureTradeBox(const Mobile& m) noexcept
{
    for (Serial s : m._contents)
    {
        Item* it = item(s);
        if (it && it->graphic == 0x1E5E && it->layer == Layer::Invalid)
            return it;
    }
    return nullptr;
}

void World::clear()
{
    _items.clear();
    _mobiles.clear();
    _player = nullptr;
    mapIndex = -1;
    light = {};
    lockedFeatures = 0;
    party.clear();
    target = {};
    prompt = {};
    _corpses.clear();
    _opl.clear();
    _clilocRequests.clear();
    _customHouseRequests.clear();
    season = Season::Summer;
    oldSeason = Season::Summer;
    _journal.clear();
    activeSpellIcons.clear();
    spellbooks.clear();
    customHouseRevisions.clear();
    skillsRequested = false;
    weather.reset();
}

void World::clearForMapChange()
{
    std::vector<Serial> toRemove;
    for (auto& [serial, it] : _items)
    {
        if (_player && rootContainer(*it) == _player->serial)
            continue;
        toRemove.push_back(serial);
    }
    for (Serial s : toRemove)
        removeItem(s);

    toRemove.clear();
    for (auto& [serial, m] : _mobiles)
    {
        if (m.get() != _player)
            toRemove.push_back(serial);
    }
    for (Serial s : toRemove)
        removeMobile(s);
}

void World::setMapIndex(int index)
{
    if (index == mapIndex)
        return;

    clearForMapChange();

    if (index < 0)
    {
        mapIndex = -1;
        return;
    }

    constexpr int kMapsCount = 6; // MapLoader.MAPS_COUNT
    mapIndex = index >= kMapsCount ? 0 : index;
    if (_player)
        _player->clearSteps();
    _listener->onMapChanged(mapIndex);
}

void World::changeSeason(Season s, int music)
{
    season = s;
    _listener->onSeasonChanged(s, music);
}

const ObjectProperty* World::properties(Serial serial) const noexcept
{
    auto it = _opl.find(serial);
    return it == _opl.end() ? nullptr : &it->second;
}

void World::setProperties(Serial serial, ObjectProperty prop)
{
    _opl[serial] = std::move(prop);
}

bool World::isRevisionEqual(Serial serial, uint32_t revision) const noexcept
{
    auto it = _opl.find(serial);
    if (it == _opl.end())
        return false;
    return (revision & ~0x40000000u) == it->second.revision || revision == it->second.revision;
}

void World::addMegaClilocRequest(Serial serial)
{
    if (std::find(_clilocRequests.begin(), _clilocRequests.end(), serial) == _clilocRequests.end())
        _clilocRequests.push_back(serial);
}

void World::addCustomHouseRequest(Serial serial)
{
    _customHouseRequests.push_back(serial);
}

void World::requestMobileStatus(Serial serial, bool force)
{
    if (!inGame())
        return;

    if (Entity* ent = get(serial))
    {
        if (force && ent->hitsRequest >= HitsRequest::Pending)
            closeStatus(serial);

        if (ent->hitsRequest < HitsRequest::Received)
        {
            ent->hitsRequest = HitsRequest::Pending;
            force = true;
        }
    }

    if (force && isValidSerial(serial))
        _requests->requestMobileStatus(serial);
}

void World::closeStatus(Serial serial, bool force)
{
    if (clientVersion < versions::CV_200 || !inGame())
        return;

    if (Entity* ent = get(serial); ent && ent->hitsRequest >= HitsRequest::Pending)
    {
        ent->hitsRequest = HitsRequest::None;
        force = true;
    }

    if (force && isValidSerial(serial))
        _requests->closeStatus(serial);
}

void World::flushPendingRequests()
{
    if (tooltipsEnabled && !_clilocRequests.empty())
    {
        _requests->megaClilocRequest(_clilocRequests);
        _clilocRequests.clear();
    }

    for (Serial s : _customHouseRequests)
        _requests->customHouseDataRequest(s);
    _customHouseRequests.clear();
}

void World::addCorpse(Serial corpse, Serial owner, Direction dir, bool running)
{
    for (const auto& c : _corpses)
    {
        if (c.corpse == corpse)
            return;
    }
    _corpses.push_back({corpse, owner, dir, running});
}

bool World::corpseExists(Serial corpse, Serial owner) const noexcept
{
    for (const auto& c : _corpses)
    {
        if (c.corpse == corpse || c.owner == owner)
            return true;
    }
    return false;
}

void World::removeCorpse(Serial corpse, Serial owner)
{
    for (auto it = _corpses.begin(); it != _corpses.end();)
    {
        if (it->corpse == corpse || it->owner == owner)
        {
            if (corpse != 0)
            {
                if (Item* i = item(corpse))
                    i->layer = Layer(uint8_t(directionMasked(it->direction)) | (it->running ? 0x80 : 0));
            }
            it = _corpses.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

void World::addMessage(Message msg)
{
    // Cliloc messages may arrive untranslated (no resolver): they carry the number instead.
    if (msg.text.empty() && msg.cliloc == 0)
        return;
    _journal.push_back(std::move(msg));
    while (_journal.size() > kJournalLimit)
        _journal.pop_front();
    _listener->onMessage(_journal.back());
}

void World::setTargeting(CursorTarget state, uint32_t cursorId, TargetType type)
{
    if (state == CursorTarget::Invalid)
        return;

    const bool wasTargeting = target.isTargeting;
    target.isTargeting = type < TargetType::Cancel;
    target.state = state;
    target.type = type;

    if (!target.isTargeting && wasTargeting)
        cancelTarget();

    // A server cancel must echo the last active cursor id, so update it last.
    target.cursorId = cursorId;
    _listener->onTargetCursorChanged();
}

void World::setTargetingMulti(uint32_t deedSerial, uint16_t model, uint16_t x, uint16_t y, uint16_t z, uint16_t hue)
{
    setTargeting(CursorTarget::MultiPlacement, deedSerial, TargetType::Neutral);
    target.multi = MultiTarget{model, x, y, z, hue};
    _listener->onTargetCursorChanged();
}

void World::cancelTarget()
{
    target.multi.reset();
    target.isTargeting = false;
}

void World::moveMobile(Mobile& m, uint16_t x, uint16_t y, int8_t z, Direction dir)
{
    const Direction clean = directionMasked(dir);
    const bool run = directionRunning(dir);

    if (!m.enqueueStep(x, y, z, clean, run))
    {
        m.x = x;
        m.y = y;
        m.z = z;
        m.direction = clean;
        m.isRunning = run;
        m.clearSteps();
    }
}

} // namespace uo::world
