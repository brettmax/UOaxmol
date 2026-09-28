// SPDX-License-Identifier: BSD-2-Clause
// Ported from ClassicUO (ClassicUO.Client/Network/PacketHandlers.cs): dispatch, entity,
// container, equipment and status handlers. UI-facing handlers are in PacketHandlersUi.cpp.
#include "uo/world/PacketHandlers.h"

#include "PacketHandlersInternal.h"
#include "uo/net/PacketTable.h"

#include <algorithm>

namespace uo::world
{

using io::BinaryReader;

PacketHandlers::PacketHandlers()
{
    add(0x1B, enterWorld);
    add(0x55, loginComplete);
    add(0xBD, clientVersion);
    add(0x03, noop); // ClientTalk
    add(0x0B, damage);
    add(0x11, characterStatus);
    add(0x15, noop); // FollowR
    add(0x16, newHealthbarUpdate);
    add(0x17, newHealthbarUpdate);
    add(0x1A, updateItem);
    add(0x1C, talk);
    add(0x1D, deleteObject);
    add(0x20, updatePlayerPacket);
    add(0x23, dragAnimation);
    add(0x24, openContainer);
    add(0x25, updateContainedItem);
    add(0x27, denyMoveItem);
    add(0x28, endDraggingItem);
    add(0x29, dropItemAccepted);
    add(0x2C, deathScreen);
    add(0x2D, mobileAttributes);
    add(0x2E, equipItem);
    add(0x32, noop);
    add(0x3A, updateSkills);
    add(0x3B, closeVendorInterface);
    add(0x3C, updateContainedItems);
    add(0x4E, personalLightLevel);
    add(0x4F, lightLevel);
    add(0x54, playSoundEffect);
    add(0x56, mapData);
    add(0x5B, noop); // SetTime
    add(0x65, setWeather);
    add(0x66, bookData);
    add(0x6C, targetCursor);
    add(0x6D, playMusic);
    add(0x6E, characterAnimation);
    add(0x6F, secureTrading);
    add(0x70, graphicEffect);
    add(0x71, bulletinBoardData);
    add(0x72, warmode);
    add(0x73, ping);
    add(0x74, buyList);
    add(0x77, updateCharacter);
    add(0x78, updateObject);
    add(0x7C, openMenu);
    add(0x88, openPaperdoll);
    add(0x89, corpseEquipment);
    add(0x90, displayMap);
    add(0x93, openBook);
    add(0x95, dyeData);
    add(0x98, updateName);
    add(0x99, multiPlacement);
    add(0x9A, asciiPrompt);
    add(0x9E, sellList);
    add(0xA1, updateHitpoints);
    add(0xA2, updateMana);
    add(0xA3, updateStamina);
    add(0xA5, openUrl);
    add(0xA6, tipWindow);
    add(0xAA, attackCharacter);
    add(0xAB, textEntryDialog);
    add(0xAE, unicodeTalk);
    add(0xAF, displayDeath);
    add(0xB2, chatMessage);
    add(0xB7, noop); // Help
    add(0xB8, characterProfile);
    add(0xB9, enableLockedFeatures);
    add(0xBA, displayQuestArrow);
    add(0xBB, noop); // UltimaMessengerR
    add(0xBC, season);
    add(0xBE, noop); // AssistVersion
    add(0xBF, extendedCommand);
    add(0xC0, graphicEffect);
    add(0xC1, displayClilocString);
    add(0xC2, unicodePrompt);
    add(0xC4, noop); // Semivisible
    add(0xC6, noop); // InvalidMapEnable
    add(0xC7, graphicEffect);
    add(0xC8, clientViewRange);
    add(0xCA, noop);
    add(0xCB, noop);
    add(0xCC, displayClilocString);
    add(0xD0, noop);
    add(0xD1, logout);
    add(0xD2, updateCharacter);
    add(0xD3, updateObject);
    add(0xD4, openBook);
    add(0xD6, megaCliloc);
    add(0xD7, noop); // GenericAOSCommandsR
    add(0xD8, customHouse);
    add(0xDB, noop); // CharacterTransferLog
    add(0xDC, oplInfo);
    add(0xDE, noop); // UpdateMobileStatus: read-only in ClassicUO
    add(0xDF, buffDebuff);
    add(0xE2, newCharacterAnimation);
    add(0xE3, noop); // KREncryptionResponse
    add(0xE5, displayWaypoint);
    add(0xE6, removeWaypoint);
    add(0xF0, krriosClientSpecial);
    add(0xF1, noop); // FreeshardListR
    add(0xF3, updateItemSA);
    add(0xF5, displayMap);
    add(0xF6, boatMoving);
    add(0xF7, packetList);
}

bool PacketHandlers::handle(World& world, std::span<const uint8_t> packet, const net::PacketTable& table)
{
    if (packet.empty())
        return false;
    return handle(world, packet, table.length(packet[0]) == -1 ? 3 : 1);
}

bool PacketHandlers::handle(World& world, std::span<const uint8_t> packet, size_t headerSize)
{
    if (packet.empty())
        return false;

    const uint8_t id = packet[0];
    BinaryReader r(packet);
    r.seek(headerSize);

    if (_hooks[id])
    {
        _hooks[id](world, r);
        return true;
    }

    if (!_handlers[id])
        return false;

    _handlers[id](*this, world, r);
    return true;
}

void PacketHandlers::noop(PacketHandlers&, World&, BinaryReader&) {}

// ---------------------------------------------------------------------------------------------
// Shared helpers
// ---------------------------------------------------------------------------------------------

void updateGameObject(World& world, Serial serial, uint16_t graphic, uint8_t graphicInc, uint16_t count, uint16_t x,
                      uint16_t y, int8_t z, Direction dir, uint16_t hue, uint8_t flagBits, uint8_t type)
{
    Entity* obj = world.get(serial);
    Mobile* mobile = nullptr;
    Item* item = nullptr;
    bool created = false;

    if (!obj)
    {
        created = true;

        if (isMobileSerial(serial) && type != 3)
        {
            mobile = &world.getOrCreateMobile(serial);
            obj = mobile;
            mobile->graphic = uint16_t(graphic + graphicInc);
            mobile->direction = directionMasked(dir);
            mobile->fixHue(hue);
            mobile->x = x;
            mobile->y = y;
            mobile->z = z;
            mobile->flagBits = flagBits;
        }
        else
        {
            item = &world.getOrCreateItem(serial);
            obj = item;
        }
    }
    else if (obj->isItem())
    {
        item = static_cast<Item*>(obj);
        if (isValidSerial(item->container))
            world.removeItemFromContainer(*item);
    }
    else
    {
        mobile = static_cast<Mobile*>(obj);
    }

    if (item)
    {
        if (graphic != kCorpseGraphic)
            graphic = uint16_t(graphic + graphicInc);

        if (type == 2)
        {
            item->isMulti = true;
            item->wantUpdateMulti = (graphic & 0x3FFF) != item->graphic || item->x != x || item->y != y ||
                                    item->z != z || item->hue != hue;
            item->graphic = uint16_t(graphic & 0x3FFF);
        }
        else
        {
            item->isDamageable = type == 3;
            item->isMulti = false;
            item->graphic = graphic;
        }

        item->x = x;
        item->y = y;
        item->z = z;
        item->lightId = uint8_t(dir);

        if (graphic == kCorpseGraphic)
            item->layer = Layer(uint8_t(dir));

        item->fixHue(hue);
        item->amount = count == 0 ? 1 : count;
        item->flagBits = flagBits;
        item->direction = dir;
    }
    else
    {
        graphic = uint16_t(graphic + graphicInc);

        if (!world.isPlayer(serial))
            world.moveMobile(*mobile, x, y, z, dir);

        mobile->graphic = uint16_t(graphic & 0x3FFF);
        mobile->fixHue(hue);
        mobile->flagBits = flagBits;
    }

    if (created)
    {
        world.listener().onEntityCreated(*obj);
        // ClassicUO asks for every new mobile's hits so health bars are populated.
        if (mobile)
            world.requestMobileStatus(serial);
    }
    else
    {
        world.listener().onEntityUpdated(*obj);
    }
}

void updatePlayer(World& world, Serial serial, uint16_t graphic, uint16_t hue, uint8_t flagBits, uint16_t x, uint16_t y,
                  int8_t z, Direction dir)
{
    Player* p = world.player();
    if (!p || p->serial != serial)
        return;

    world.rangeX = x;
    world.rangeY = y;

    const bool wasDead = p->isDead();

    p->graphic = graphic;
    p->direction = directionMasked(dir);
    p->fixHue(hue);
    p->flagBits = flagBits;
    world.weather.reset();

    if (wasDead != p->isDead())
    {
        if (p->isDead())
            world.changeSeason(Season::Desolation, 42);
        else
            world.changeSeason(world.oldSeason, world.oldMusicIndex);
    }

    p->x = x;
    p->y = y;
    p->z = z;
    p->clearSteps();

    world.listener().onPlayerTeleported(*p);
    world.listener().onEntityUpdated(*p);
    world.listener().onPlayerWeaponChanged(*p);
}

void addItemToContainer(World& world, Serial serial, uint16_t graphic, uint16_t amount, uint16_t x, uint16_t y,
                        uint16_t hue, Serial containerSerial)
{
    Entity* container = world.get(containerSerial);
    if (!container)
        return; // ClassicUO: "No container found"

    if (isMobileSerial(serial))
        world.removeMobile(serial);

    Item* existing = world.item(serial);
    if (existing && (container->graphic != kCorpseGraphic || existing->layer == Layer::Invalid))
        world.removeItem(serial);

    Item& item = world.getOrCreateItem(serial);
    item.graphic = graphic;
    item.amount = amount;
    item.fixHue(hue);
    item.x = x;
    item.y = y;
    item.z = 0;

    world.removeItemFromContainer(item);
    item.container = containerSerial;
    world.pushToBack(*container, item);

    if (isItemSerial(containerSerial))
    {
        if (auto* c = world.item(containerSerial))
        {
            // A bulletin board lists messages as contained items and wants their summaries.
            if (c->graphic == 0x0EB0 && c->opened)
                world.requests().bulletinSummaryRequest(containerSerial, serial);
        }
    }

    world.listener().onEntityUpdated(item);
    if (isMobileSerial(containerSerial))
        world.listener().onEquipmentChanged(containerSerial);
    else
        world.listener().onContainerContentsChanged(containerSerial);
}

// ---------------------------------------------------------------------------------------------
// Entities
// ---------------------------------------------------------------------------------------------

void PacketHandlers::enterWorld(PacketHandlers&, World& world, BinaryReader& r)
{
    const Serial serial = r.readU32BE();
    Player& p = world.createPlayer(serial);

    r.skip(4);
    p.graphic = r.readU16BE();
    const uint16_t x = r.readU16BE();
    const uint16_t y = r.readU16BE();
    const auto z = int8_t(r.readU16BE());

    if (world.mapIndex < 0)
        world.setMapIndex(0);

    p.x = x;
    p.y = y;
    p.z = z;
    p.direction = Direction(r.readU8() & 0x07);
    world.rangeX = x;
    world.rangeY = y;

    world.requests().clientVersion();
    world.requests().singleClick(serial);
    world.requests().requestSkills(serial);

    if (p.isDead())
        world.changeSeason(Season::Desolation, 42);

    world.listener().onEnterWorld(p);
}

void PacketHandlers::loginComplete(PacketHandlers&, World& world, BinaryReader&)
{
    Player* p = world.player();
    if (!p)
        return;

    world.requestMobileStatus(p->serial);
    world.requests().openChat("");
    world.requests().requestSkills(p->serial);
    world.requests().doubleClick(p->serial);

    if (world.clientVersion >= versions::CV_306E)
        world.requests().clientType();
    if (world.clientVersion >= versions::CV_305D)
        world.requests().clientViewRange(world.clientViewRange);

    world.listener().onLoginComplete(*p);
}

void PacketHandlers::clientVersion(PacketHandlers&, World& world, BinaryReader&)
{
    world.requests().clientVersion();
}

void PacketHandlers::damage(PacketHandlers&, World& world, BinaryReader& r)
{
    if (!world.player())
        return;

    const Serial serial = r.readU32BE();
    if (world.get(serial))
    {
        const uint16_t amount = r.readU16BE();
        if (amount > 0)
            world.listener().onDamage(serial, amount);
    }
}

void PacketHandlers::characterStatus(PacketHandlers&, World& world, BinaryReader& r)
{
    if (!world.player())
        return;

    const Serial serial = r.readU32BE();
    Entity* entity = world.get(serial);
    if (!entity)
        return;

    entity->name = detail::ascii(r, 30);
    entity->hits = r.readU16BE();
    entity->hitsMax = r.readU16BE();
    if (entity->hitsRequest == HitsRequest::Pending)
        entity->hitsRequest = HitsRequest::Received;

    if (isMobileSerial(serial) && entity->isMobile())
    {
        auto& mobile = static_cast<Mobile&>(*entity);
        mobile.isRenamable = (r.readU8() != 0);
        const uint8_t type = r.readU8();

        if (type > 0 && r.position() + 1 <= r.size())
        {
            mobile.isFemale = (r.readU8() != 0);

            if (world.isPlayer(serial))
            {
                Player& p = *world.player();
                p.strength = r.readU16BE();
                p.dexterity = r.readU16BE();
                p.intelligence = r.readU16BE();
                p.stamina = r.readU16BE();
                p.staminaMax = r.readU16BE();
                p.mana = r.readU16BE();
                p.manaMax = r.readU16BE();
                p.gold = r.readU32BE();
                p.physicalResistance = r.readI16BE();
                p.weight = r.readU16BE();

                if (type >= 5) // ML
                {
                    p.weightMax = r.readU16BE();
                    uint8_t race = r.readU8();
                    p.race = Race(race == 0 ? 1 : race);
                }
                else if (world.clientVersion >= versions::CV_500A)
                {
                    p.weightMax = uint16_t(7 * (p.strength >> 1) + 40);
                }
                else
                {
                    p.weightMax = uint16_t(p.strength * 4 + 25);
                }

                if (type >= 3) // Renaissance
                {
                    p.statsCap = r.readI16BE();
                    p.followers = r.readU8();
                    p.followersMax = r.readU8();
                }

                if (type >= 4) // AOS
                {
                    p.fireResistance = r.readI16BE();
                    p.coldResistance = r.readI16BE();
                    p.poisonResistance = r.readI16BE();
                    p.energyResistance = r.readI16BE();
                    p.luck = r.readU16BE();
                    p.damageMin = r.readI16BE();
                    p.damageMax = r.readI16BE();
                    p.tithingPoints = r.readU32BE();
                }

                if (type >= 6)
                {
                    auto opt = [&r]() -> int16_t { return r.position() + 2 > r.size() ? 0 : r.readI16BE(); };
                    p.maxPhysicalResistance = opt();
                    p.maxFireResistance = opt();
                    p.maxColdResistance = opt();
                    p.maxPoisonResistance = opt();
                    p.maxEnergyResistance = opt();
                    p.defenseChanceIncrease = opt();
                    p.maxDefenseChanceIncrease = opt();
                    p.hitChanceIncrease = opt();
                    p.swingSpeedIncrease = opt();
                    p.damageIncrease = opt();
                    p.lowerReagentCost = opt();
                    p.spellDamageIncrease = opt();
                    p.fasterCastRecovery = opt();
                    p.fasterCasting = opt();
                    p.lowerManaCost = opt();
                }
            }
        }
    }

    world.listener().onNameChanged(*entity);
    world.listener().onStatsChanged(*entity);
}

void PacketHandlers::newHealthbarUpdate(PacketHandlers&, World& world, BinaryReader& r)
{
    if (!world.player())
        return;
    if (r.buffer()[0] == 0x16 && world.clientVersion < versions::CV_500A)
        return;

    Mobile* mobile = world.mobile(r.readU32BE());
    if (!mobile)
        return;

    const uint16_t count = r.readU16BE();
    for (uint16_t i = 0; i < count; ++i)
    {
        const uint16_t type = r.readU16BE();
        const bool enabled = (r.readU8() != 0);

        if (type == 1)
        {
            if (world.clientVersion >= versions::CV_7000)
                mobile->setSAPoison(enabled);
            else if (enabled)
                mobile->flagBits |= flags::Poisoned;
            else
                mobile->flagBits &= uint8_t(~flags::Poisoned);
        }
        else if (type == 2)
        {
            if (enabled)
                mobile->flagBits |= flags::YellowBar;
            else
                mobile->flagBits &= uint8_t(~flags::YellowBar);
        }
    }

    world.listener().onStatsChanged(*mobile);
}

void PacketHandlers::updateItem(PacketHandlers&, World& world, BinaryReader& r)
{
    if (!world.player())
        return;

    Serial serial = r.readU32BE();
    uint16_t count = 0;
    uint8_t graphicInc = 0;
    uint8_t direction = 0;
    uint16_t hue = 0;
    uint8_t flagBits = 0;
    uint8_t type = 0;

    if (serial & 0x80000000u)
    {
        serial &= 0x7FFFFFFFu;
        count = 1;
    }

    uint16_t graphic = r.readU16BE();
    if (graphic & 0x8000)
    {
        graphic &= 0x7FFF;
        graphicInc = r.readU8();
    }

    count = count > 0 ? r.readU16BE() : uint16_t(count + 1);

    uint16_t x = r.readU16BE();
    if (x & 0x8000)
    {
        x &= 0x7FFF;
        direction = 1;
    }

    uint16_t y = r.readU16BE();
    if (y & 0x8000)
    {
        y &= 0x7FFF;
        hue = 1;
    }
    if (y & 0x4000)
    {
        y &= 0x3FFF;
        flagBits = 1;
    }

    if (direction)
        direction = r.readU8();
    const int8_t z = r.readI8();
    if (hue)
        hue = r.readU16BE();
    if (flagBits)
        flagBits = r.readU8();

    if (graphic >= 0x4000)
        type = 2;

    updateGameObject(world, serial, graphic, graphicInc, count, x, y, z, Direction(direction), hue, flagBits, type);
}

void PacketHandlers::updateItemSA(PacketHandlers&, World& world, BinaryReader& r)
{
    if (!world.player())
        return;

    const bool fromPacketList = r.buffer()[0] == 0xF7;
    r.skip(2); // constant 0x0001

    const uint8_t type = r.readU8();
    const Serial serial = r.readU32BE();
    const uint16_t graphic = r.readU16BE();
    const uint8_t graphicInc = r.readU8();
    const uint16_t amount = r.readU16BE();
    r.readU16BE(); // unknown
    const uint16_t x = r.readU16BE();
    const uint16_t y = r.readU16BE();
    const int8_t z = r.readI8();
    const auto dir = Direction(r.readU8());
    const uint16_t hue = r.readU16BE();
    const uint8_t flagBits = r.readU8();
    r.readU16BE(); // unknown

    if (!world.isPlayer(serial))
        updateGameObject(world, serial, graphic, graphicInc, amount, x, y, z, dir, hue, flagBits, type);
    else if (fromPacketList)
        updatePlayer(world, serial, graphic, hue, flagBits, x, y, z, dir);
}

void PacketHandlers::packetList(PacketHandlers& self, World& world, BinaryReader& r)
{
    if (!world.player())
        return;

    const uint16_t count = r.readU16BE();
    for (uint16_t i = 0; i < count; ++i)
    {
        if (r.readU8() != 0xF3)
            break; // ClassicUO: unknown packet in 0xF7
        updateItemSA(self, world, r);
    }
}

void PacketHandlers::deleteObject(PacketHandlers&, World& world, BinaryReader& r)
{
    Player* player = world.player();
    if (!player)
        return;

    const Serial serial = r.readU32BE();
    if (serial == player->serial)
        return;

    Entity* entity = world.get(serial);
    if (!entity)
        return;

    bool updateAbilities = false;
    if (entity->isItem())
    {
        auto* it = static_cast<Item*>(entity);
        if (isValidSerial(it->container) && world.rootContainer(*it) == player->serial)
            updateAbilities = it->layer == Layer::OneHanded || it->layer == Layer::TwoHanded;
    }

    // A dying mobile keeps playing its death animation until its corpse arrives.
    if (world.corpseExists(0, serial))
        return;

    if (entity->isMobile())
    {
        world.removeMobile(serial);
        return;
    }

    world.removeItem(serial);
    if (updateAbilities)
        world.listener().onPlayerWeaponChanged(*player);
}

void PacketHandlers::updatePlayerPacket(PacketHandlers&, World& world, BinaryReader& r)
{
    if (!world.player())
        return;

    const Serial serial = r.readU32BE();
    const uint16_t graphic = r.readU16BE();
    r.readU8(); // graphic increment, unused for the player
    const uint16_t hue = r.readU16BE();
    const uint8_t flagBits = r.readU8();
    const uint16_t x = r.readU16BE();
    const uint16_t y = r.readU16BE();
    r.readU16BE(); // server id
    const auto dir = Direction(r.readU8());
    const int8_t z = r.readI8();

    updatePlayer(world, serial, graphic, hue, flagBits, x, y, z, dir);
}

void PacketHandlers::updateCharacter(PacketHandlers&, World& world, BinaryReader& r)
{
    if (!world.player())
        return;

    const Serial serial = r.readU32BE();
    Mobile* mobile = world.mobile(serial);
    if (!mobile)
        return;

    const uint16_t graphic = r.readU16BE();
    const uint16_t x = r.readU16BE();
    const uint16_t y = r.readU16BE();
    const int8_t z = r.readI8();
    const auto dir = Direction(r.readU8());
    const uint16_t hue = r.readU16BE();
    const uint8_t flagBits = r.readU8();
    const auto notoriety = Notoriety(r.readU8());

    mobile->notoriety = notoriety;

    if (world.isPlayer(serial))
    {
        // x/y/z/direction from 0x77 would fight the walker; ClassicUO ignores them for the player.
        mobile->flagBits = flagBits;
        mobile->graphic = graphic;
        mobile->fixHue(hue);
        world.listener().onEntityUpdated(*mobile);
    }
    else
    {
        updateGameObject(world, serial, graphic, 0, 0, x, y, z, dir, hue, flagBits, 1);
    }
}

void PacketHandlers::updateObject(PacketHandlers&, World& world, BinaryReader& r)
{
    Player* player = world.player();
    if (!player)
        return;

    const uint8_t id = r.buffer()[0];
    const Serial serial = r.readU32BE();
    const uint16_t graphic = r.readU16BE();
    const uint16_t x = r.readU16BE();
    const uint16_t y = r.readU16BE();
    const int8_t z = r.readI8();
    const auto dir = Direction(r.readU8());
    const uint16_t hue = r.readU16BE();
    const uint8_t flagBits = r.readU8();
    const auto notoriety = Notoriety(r.readU8());
    bool wasDead = false;

    if (serial == player->serial)
    {
        wasDead = player->isDead();
        player->graphic = graphic;
        player->fixHue(hue);
        player->flagBits = flagBits;
    }
    else
    {
        updateGameObject(world, serial, graphic, 0, 0, x, y, z, dir, hue, flagBits, 0);
    }

    Entity* obj = world.get(serial);
    if (!obj)
        return;

    // Equipment is resent in full; drop what is not an opened container or the backpack.
    {
        const std::vector<Serial> children = obj->contents();
        for (Serial child : children)
        {
            Item* it = world.item(child);
            if (it && !it->opened && it->layer != Layer::Backpack)
                world.removeItem(child);
        }
    }

    if (obj->isMobile())
    {
        // updateGameObject already notified; notoriety arrives after it, so tell the view again.
        static_cast<Mobile*>(obj)->notoriety = notoriety;
        world.listener().onEntityUpdated(*obj);
    }

    if (id != 0x78)
        r.skip(6);

    Serial itemSerial = r.readU32BE();
    while (itemSerial != 0 && r.position() < r.size())
    {
        uint16_t itemGraphic = r.readU16BE();
        const uint8_t layer = r.readU8();
        uint16_t itemHue = 0;

        if (world.clientVersion >= versions::CV_70331)
        {
            itemHue = r.readU16BE();
        }
        else if (itemGraphic & 0x8000)
        {
            itemGraphic &= 0x7FFF;
            itemHue = r.readU16BE();
        }

        Item& item = world.getOrCreateItem(itemSerial);
        item.graphic = itemGraphic;
        item.fixHue(itemHue);
        item.amount = 1;
        world.removeItemFromContainer(item);
        item.container = serial;
        item.layer = Layer(layer);
        world.pushToBack(*obj, item);
        world.listener().onEntityUpdated(item);

        itemSerial = r.readU32BE();
    }

    if (serial == player->serial)
    {
        if (wasDead != player->isDead())
        {
            if (player->isDead())
                world.changeSeason(Season::Desolation, 42);
            else
                world.changeSeason(world.oldSeason, world.oldMusicIndex);
        }
        world.listener().onEntityUpdated(*player);
        world.listener().onPlayerWeaponChanged(*player);
    }

    if (obj->isMobile())
        world.listener().onEquipmentChanged(serial);
}

void PacketHandlers::equipItem(PacketHandlers&, World& world, BinaryReader& r)
{
    if (!world.inGame())
        return;

    const Serial serial = r.readU32BE();
    Item& item = world.getOrCreateItem(serial);

    if (item.graphic != 0 && item.layer != Layer::Backpack)
        world.removeItemFromContainer(item);

    item.graphic = uint16_t(r.readU16BE() + r.readI8());
    item.layer = Layer(r.readU8());
    item.container = r.readU32BE();
    item.fixHue(r.readU16BE());
    item.amount = 1;

    Entity* owner = world.get(item.container);
    if (owner)
        world.pushToBack(*owner, item);

    world.listener().onEntityUpdated(item);
    if (isValidSerial(item.container) && item.layer < Layer::Mount)
        world.listener().onEquipmentChanged(item.container);

    if (owner && world.isPlayer(owner->serial) && (item.layer == Layer::OneHanded || item.layer == Layer::TwoHanded))
        world.listener().onPlayerWeaponChanged(*world.player());
}

void PacketHandlers::corpseEquipment(PacketHandlers&, World& world, BinaryReader& r)
{
    if (!world.inGame())
        return;

    const Serial serial = r.readU32BE();
    Entity* corpse = world.get(serial);
    if (!corpse || corpse->graphic != kCorpseGraphic)
        return;

    uint8_t layer = r.readU8();
    while (layer != uint8_t(Layer::Invalid) && r.position() < r.size())
    {
        const Serial itemSerial = r.readU32BE();

        // The corpse packet's layers are one higher than equipment layers.
        if (uint8_t(layer - 1) != uint8_t(Layer::Backpack))
        {
            Item& item = world.getOrCreateItem(itemSerial);
            world.removeItemFromContainer(item);
            item.container = serial;
            item.layer = Layer(uint8_t(layer - 1));
            world.pushToBack(*corpse, item);
        }

        layer = r.readU8();
    }

    world.listener().onContainerContentsChanged(serial);
}

void PacketHandlers::displayDeath(PacketHandlers&, World& world, BinaryReader& r)
{
    if (!world.inGame())
        return;

    Serial serial = r.readU32BE();
    const Serial corpseSerial = r.readU32BE();
    const uint32_t running = r.readU32BE();

    Mobile* owner = world.mobile(serial);
    if (!owner || world.isPlayer(serial))
        return;

    serial |= 0x80000000u;
    world.rekeyMobile(owner->serial, serial);
    owner = world.mobile(serial);

    if (isValidSerial(corpseSerial))
        world.addCorpse(corpseSerial, serial, owner->direction, running != 0);

    auto& anim = owner->animation;
    anim.source = ServerAnimation::Source::Death;
    anim.running = running != 0;
    anim.frameCount = 5;
    anim.repeatCount = 1;
    ++anim.sequence;

    world.listener().onMobileDied(*owner, corpseSerial, running != 0);
    world.listener().onAnimationRequested(*owner);
}

// ---------------------------------------------------------------------------------------------
// Containers
// ---------------------------------------------------------------------------------------------

void PacketHandlers::openContainer(PacketHandlers&, World& world, BinaryReader& r)
{
    if (!world.player())
        return;

    const Serial serial = r.readU32BE();
    const uint16_t graphic = r.readU16BE();

    if (graphic == 0x0030)
    {
        Mobile* vendor = world.mobile(serial);
        if (!vendor)
            return;

        ShopData shop;
        shop.vendor = serial;
        shop.isBuy = true;

        for (auto layer = uint8_t(Layer::ShopBuyRestock); layer <= uint8_t(Layer::ShopBuy); ++layer)
        {
            Item* box = world.findItemByLayer(*vendor, Layer(layer));
            if (!box || box->isEmpty())
                continue;

            // The original client lists restock/buy boxes back to front except for 0x2AF8.
            const bool reverse = box->graphic != 0x2AF8;
            std::vector<Serial> order = box->contents();
            if (reverse)
                std::reverse(order.begin(), order.end());

            for (Serial s : order)
            {
                Item* it = world.item(s);
                if (!it)
                    continue;
                shop.entries.push_back({it->serial, it->graphic, it->hue, it->amount, it->price, it->name, false, 0});
            }
        }

        world.listener().onOpenShop(shop);
        return;
    }

    Item* item = world.item(serial);
    if (!item)
        return;

    item->opened = true;

    if (graphic == 0xFFFF)
    {
        world.listener().onOpenSpellbook(*item);
        return;
    }

    // Contents are resent right after this packet (0x3C).
    if (!item->isCorpse())
        world.clearContainer(*item);

    world.listener().onOpenContainer(*item, graphic);
}

void PacketHandlers::updateContainedItem(PacketHandlers&, World& world, BinaryReader& r)
{
    if (!world.inGame())
        return;

    const Serial serial = r.readU32BE();
    const auto graphic = uint16_t(r.readU16BE() + r.readU8());
    const uint16_t amount = std::max<uint16_t>(1, r.readU16BE());
    const uint16_t x = r.readU16BE();
    const uint16_t y = r.readU16BE();
    if (world.clientVersion >= versions::CV_6017)
        r.skip(1); // grid index
    const Serial containerSerial = r.readU32BE();
    const uint16_t hue = r.readU16BE();

    addItemToContainer(world, serial, graphic, amount, x, y, hue, containerSerial);
}

void PacketHandlers::updateContainedItems(PacketHandlers&, World& world, BinaryReader& r)
{
    if (!world.inGame())
        return;

    const uint16_t count = r.readU16BE();
    for (uint16_t i = 0; i < count; ++i)
    {
        const Serial serial = r.readU32BE();
        const auto graphic = uint16_t(r.readU16BE() + r.readU8());
        const uint16_t amount = std::max<uint16_t>(1, r.readU16BE());
        const uint16_t x = r.readU16BE();
        const uint16_t y = r.readU16BE();
        if (world.clientVersion >= versions::CV_6017)
            r.skip(1);
        const Serial containerSerial = r.readU32BE();
        const uint16_t hue = r.readU16BE();

        if (i == 0)
        {
            if (Entity* container = world.get(containerSerial))
                world.clearContainer(*container, container->graphic == kCorpseGraphic);
        }

        addItemToContainer(world, serial, graphic, amount, x, y, hue, containerSerial);
    }
}

void PacketHandlers::denyMoveItem(PacketHandlers&, World& world, BinaryReader& r)
{
    if (!world.inGame())
        return;

    const uint8_t code = r.readU8();
    world.listener().onDropRejected(code);

    if (code < 5)
    {
        static constexpr const char* kFallback[5] = {
            "You can not pick that up.",
            "That is too far away.",
            "That is out of sight.",
            "That item does not belong to you.  You'll have to steal it.",
            "You are already holding an item.",
        };

        Message msg;
        msg.hue = 0x03B2;
        msg.type = MessageType::System;
        msg.font = 3;
        msg.textType = TextType::System;
        msg.cliloc = 3000267u + code;
        msg.text = detail::clilocOr(world, msg.cliloc, kFallback[code]);
        world.addMessage(std::move(msg));
    }
}

void PacketHandlers::restoreHeldItem(World& world, const HeldItem& held)
{
    if (!isValidSerial(held.serial) || held.graphic == 0xFFFF || held.updatedInWorld)
        return;

    if (held.layer == Layer::Invalid && isValidSerial(held.container))
    {
        // The server normally follows up with 0x25; restore it now so the item never vanishes.
        addItemToContainer(world, held.serial, held.graphic, held.amount, held.x, held.y, held.hue, held.container);
        return;
    }

    Item& item = world.getOrCreateItem(held.serial);
    item.graphic = held.graphic;
    item.hue = held.hue;
    item.amount = held.amount;
    item.flagBits = held.flags;
    item.layer = held.layer;
    item.x = held.x;
    item.y = held.y;
    item.z = held.z;

    Entity* container = world.get(held.container);
    if (container)
    {
        if (isMobileSerial(container->serial))
        {
            world.removeItemFromContainer(item);
            world.pushToBack(*container, item);
            item.container = container->serial;
            world.listener().onEquipmentChanged(container->serial);
        }
        else
        {
            world.removeItem(held.serial);
        }
    }
    else
    {
        world.removeItemFromContainer(item);
        world.listener().onEntityUpdated(item);
    }
}

void PacketHandlers::endDraggingItem(PacketHandlers&, World& world, BinaryReader&)
{
    if (world.inGame())
        world.listener().onDragEnded();
}

void PacketHandlers::dropItemAccepted(PacketHandlers&, World& world, BinaryReader&)
{
    if (world.inGame())
        world.listener().onDropAccepted();
}

// ---------------------------------------------------------------------------------------------
// Status
// ---------------------------------------------------------------------------------------------

void PacketHandlers::mobileAttributes(PacketHandlers&, World& world, BinaryReader& r)
{
    const Serial serial = r.readU32BE();
    Entity* entity = world.get(serial);
    if (!entity)
        return;

    entity->hitsMax = r.readU16BE();
    entity->hits = r.readU16BE();
    if (entity->hitsRequest == HitsRequest::Pending)
        entity->hitsRequest = HitsRequest::Received;

    if (isMobileSerial(serial) && entity->isMobile())
    {
        auto& m = static_cast<Mobile&>(*entity);
        m.manaMax = r.readU16BE();
        m.mana = r.readU16BE();
        m.staminaMax = r.readU16BE();
        m.stamina = r.readU16BE();
    }

    world.listener().onStatsChanged(*entity);
}

void PacketHandlers::updateHitpoints(PacketHandlers&, World& world, BinaryReader& r)
{
    Entity* entity = world.get(r.readU32BE());
    if (!entity)
        return;

    entity->hitsMax = r.readU16BE();
    entity->hits = r.readU16BE();
    if (entity->hitsRequest == HitsRequest::Pending)
        entity->hitsRequest = HitsRequest::Received;

    world.listener().onStatsChanged(*entity);
}

void PacketHandlers::updateMana(PacketHandlers&, World& world, BinaryReader& r)
{
    Mobile* m = world.mobile(r.readU32BE());
    if (!m)
        return;
    m->manaMax = r.readU16BE();
    m->mana = r.readU16BE();
    world.listener().onStatsChanged(*m);
}

void PacketHandlers::updateStamina(PacketHandlers&, World& world, BinaryReader& r)
{
    Mobile* m = world.mobile(r.readU32BE());
    if (!m)
        return;
    m->staminaMax = r.readU16BE();
    m->stamina = r.readU16BE();
    world.listener().onStatsChanged(*m);
}

void PacketHandlers::updateSkills(PacketHandlers&, World& world, BinaryReader& r)
{
    if (!world.inGame())
        return;

    Player& p = *world.player();
    const uint8_t type = r.readU8();
    const bool haveCap = (type != 0 && type <= 0x03) || type == 0xDF;
    const bool isSingleUpdate = type == 0xFF || type == 0xDF;

    if (type == 0xFE)
    {
        const uint16_t count = r.readU16BE();
        world.skillNames.clear();
        p.skills.clear();

        for (uint16_t i = 0; i < count; ++i)
        {
            const bool hasButton = (r.readU8() != 0);
            const uint8_t nameLength = r.readU8();
            Skill s;
            s.name = detail::ascii(r, nameLength);
            s.hasButton = hasButton;
            world.skillNames.push_back(s.name);
            p.skills.push_back(std::move(s));
        }

        world.listener().onSkillListChanged();
        return;
    }

    const bool openWindow = !isSingleUpdate && (type == 1 || type == 3 || world.skillsRequested);
    if (openWindow)
        world.skillsRequested = false;

    int lastId = -1;
    while (r.position() < r.size())
    {
        uint16_t id = r.readU16BE();
        if (r.position() >= r.size())
            break;
        if (id == 0 && type == 0)
            break;
        if (type == 0 || type == 0x02)
            --id;

        const uint16_t realVal = r.readU16BE();
        const uint16_t baseVal = r.readU16BE();
        const auto lock = SkillLock(r.readU8());
        uint16_t cap = 1000;
        if (haveCap)
            cap = r.readU16BE();

        // Grow when the server knows more skills than skills.mul listed.
        if (id < 1024 && id >= p.skills.size())
            p.skills.resize(size_t(id) + 1);

        if (id < p.skills.size())
        {
            Skill& s = p.skills[id];
            s.baseFixed = baseVal;
            s.valueFixed = realVal;
            s.capFixed = cap;
            s.lock = lock;
            lastId = id;
        }

        if (isSingleUpdate)
            break;
    }

    world.listener().onSkillsChanged(isSingleUpdate ? lastId : -1, openWindow);
}

void PacketHandlers::warmode(PacketHandlers&, World& world, BinaryReader& r)
{
    if (!world.inGame())
        return;
    world.player()->warMode = (r.readU8() != 0);
    world.listener().onEntityUpdated(*world.player());
}

void PacketHandlers::attackCharacter(PacketHandlers&, World& world, BinaryReader& r)
{
    const Serial serial = r.readU32BE();
    world.closeStatus(world.target.lastAttack);
    world.target.lastAttack = serial;
    world.requestMobileStatus(serial);
}

void PacketHandlers::updateName(PacketHandlers&, World& world, BinaryReader& r)
{
    if (!world.inGame())
        return;

    const Serial serial = r.readU32BE();
    std::string name = detail::ascii(r, -1);

    if (Entity* e = world.get(serial))
    {
        e->name = std::move(name);
        world.listener().onNameChanged(*e);
    }
}

void PacketHandlers::characterAnimation(PacketHandlers&, World& world, BinaryReader& r)
{
    Mobile* m = world.mobile(r.readU32BE());
    if (!m)
        return;

    auto& a = m->animation;
    a.source = ServerAnimation::Source::Legacy;
    a.action = r.readU16BE();
    a.frameCount = r.readU16BE();
    a.repeatCount = r.readU16BE();
    a.forward = !(r.readU8() != 0);
    a.repeat = (r.readU8() != 0);
    a.delay = r.readU8();
    ++a.sequence;

    world.listener().onAnimationRequested(*m);
}

void PacketHandlers::newCharacterAnimation(PacketHandlers&, World& world, BinaryReader& r)
{
    if (!world.player())
        return;

    Mobile* m = world.mobile(r.readU32BE());
    if (!m)
        return;

    auto& a = m->animation;
    a.source = ServerAnimation::Source::New;
    a.type = r.readU16BE();
    a.action = r.readU16BE();
    a.mode = r.readU8();
    a.repeatCount = 1;
    a.forward = true;
    a.repeat = (a.type == 1 || a.type == 2) && m->graphic == 0x0015;
    ++a.sequence;

    world.listener().onAnimationRequested(*m);
}

void PacketHandlers::boatMoving(PacketHandlers&, World& world, BinaryReader& r)
{
    if (!world.inGame())
        return;

    const Serial serial = r.readU32BE();
    const uint8_t speed = r.readU8();
    const auto moving = directionMasked(Direction(r.readU8()));
    const auto facing = directionMasked(Direction(r.readU8()));
    const uint16_t x = r.readU16BE();
    const uint16_t y = r.readU16BE();
    const uint16_t z = r.readU16BE();

    Item* multi = world.item(serial);
    if (!multi)
        return;

    // Smooth boat movement is a rendering concern; the model snaps to the server position.
    multi->x = x;
    multi->y = y;
    multi->z = int8_t(z);
    world.listener().onBoatMoved(*multi, speed, moving, facing);
    world.listener().onEntityUpdated(*multi);

    const uint16_t count = r.readU16BE();
    for (uint16_t i = 0; i < count; ++i)
    {
        const Serial cSerial = r.readU32BE();
        const uint16_t cx = r.readU16BE();
        const uint16_t cy = r.readU16BE();
        const uint16_t cz = r.readU16BE();

        if (world.isPlayer(cSerial))
        {
            world.rangeX = cx;
            world.rangeY = cy;
        }

        Entity* ent = world.get(cSerial);
        if (!ent)
            continue;

        if (world.isPlayer(cSerial))
        {
            updatePlayer(world, cSerial, ent->graphic, ent->hue, ent->flagBits, cx, cy, int8_t(cz),
                         world.player()->direction);
        }
        else
        {
            const uint16_t amount =
                ent->graphic == kCorpseGraphic && ent->isItem() ? static_cast<Item*>(ent)->amount : uint16_t(0);
            updateGameObject(world, cSerial, ent->graphic, 0, amount, cx, cy, int8_t(cz),
                             isMobileSerial(cSerial) ? ent->direction : Direction::North, ent->hue, ent->flagBits,
                             0);
        }
    }
}

} // namespace uo::world
