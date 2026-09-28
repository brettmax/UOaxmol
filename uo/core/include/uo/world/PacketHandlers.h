// SPDX-License-Identifier: BSD-2-Clause
// Ported from ClassicUO (ClassicUO.Client/Network/PacketHandlers.cs).
//
// Applies framed server packets (from net::PacketFramer) to a world::World.
//
// Packets owned by other modules are left unregistered so the caller can route them:
//   - login flow (0x82 0x85 0x53 0x86 0x8C 0xA8 0xA9 0xFD): net::Session
//   - gumps (0xB0 0xDD): Gump UI
//   - walking (0x21 0x22 0x97 0x38 0x2F, 0xBF 0x01/0x02 fast-walk keys): Movement and input
// Any of them can be plugged in with setHook() / setExtendedHook() so one dispatcher
// serves the whole game connection.
#pragma once

#include "uo/io/BinaryReader.h"
#include "uo/world/World.h"

#include <array>
#include <cstdint>
#include <functional>
#include <span>
#include <unordered_map>
#include <unordered_set>

namespace uo::net
{
class PacketTable;
}

namespace uo::world
{

// The item on the cursor, as the client's drag-and-drop code tracks it. Used to put an item
// back where it came from when the server rejects a drop (0x27).
struct HeldItem
{
    Serial serial{0};
    uint16_t graphic{0xFFFF};
    uint16_t hue{0};
    uint16_t amount{0};
    uint8_t flags{0};
    Layer layer{Layer::Invalid};
    Serial container{0};
    uint16_t x{0};
    uint16_t y{0};
    int8_t z{0};
    bool updatedInWorld{false};
};

class PacketHandlers
{
public:
    using Hook = std::function<void(World&, io::BinaryReader&)>;

    PacketHandlers();

    // Applies one complete packet. `packet[0]` is the id; variable-length packets still carry
    // their 2-byte length. Returns false when no handler or hook is registered for the id.
    bool handle(World& world, std::span<const uint8_t> packet, const net::PacketTable& table);
    // Same, with the header size given directly (1 for fixed, 3 for variable-length packets).
    bool handle(World& world, std::span<const uint8_t> packet, size_t headerSize);

    bool handles(uint8_t id) const noexcept { return _handlers[id] != nullptr || _hooks[id] != nullptr; }

    // Routes a packet id (or a 0xBF sub-command) to another module. The reader is positioned
    // after the header (or after the 0xBF sub-command word).
    void setHook(uint8_t id, Hook hook) { _hooks[id] = std::move(hook); }
    void setExtendedHook(uint16_t subCommand, Hook hook) { _extendedHooks[subCommand] = std::move(hook); }

    // Called by the drag-and-drop code when 0x27 arrives while it still holds an item:
    // restores the item to its container, paperdoll or the ground (DenyMoveItem).
    static void restoreHeldItem(World& world, const HeldItem& held);

private:
    using Fn = void (*)(PacketHandlers&, World&, io::BinaryReader&);
    void add(uint8_t id, Fn fn) { _handlers[id] = fn; }

    // --- handlers (one per ClassicUO method) --------------------------------------------
    static void enterWorld(PacketHandlers&, World&, io::BinaryReader&);
    static void loginComplete(PacketHandlers&, World&, io::BinaryReader&);
    static void clientVersion(PacketHandlers&, World&, io::BinaryReader&);
    static void noop(PacketHandlers&, World&, io::BinaryReader&);
    static void damage(PacketHandlers&, World&, io::BinaryReader&);
    static void characterStatus(PacketHandlers&, World&, io::BinaryReader&);
    static void newHealthbarUpdate(PacketHandlers&, World&, io::BinaryReader&);
    static void updateItem(PacketHandlers&, World&, io::BinaryReader&);
    static void talk(PacketHandlers&, World&, io::BinaryReader&);
    static void deleteObject(PacketHandlers&, World&, io::BinaryReader&);
    static void updatePlayerPacket(PacketHandlers&, World&, io::BinaryReader&);
    static void dragAnimation(PacketHandlers&, World&, io::BinaryReader&);
    static void openContainer(PacketHandlers&, World&, io::BinaryReader&);
    static void updateContainedItem(PacketHandlers&, World&, io::BinaryReader&);
    static void denyMoveItem(PacketHandlers&, World&, io::BinaryReader&);
    static void endDraggingItem(PacketHandlers&, World&, io::BinaryReader&);
    static void dropItemAccepted(PacketHandlers&, World&, io::BinaryReader&);
    static void deathScreen(PacketHandlers&, World&, io::BinaryReader&);
    static void mobileAttributes(PacketHandlers&, World&, io::BinaryReader&);
    static void equipItem(PacketHandlers&, World&, io::BinaryReader&);
    static void updateSkills(PacketHandlers&, World&, io::BinaryReader&);
    static void updateContainedItems(PacketHandlers&, World&, io::BinaryReader&);
    static void closeVendorInterface(PacketHandlers&, World&, io::BinaryReader&);
    static void personalLightLevel(PacketHandlers&, World&, io::BinaryReader&);
    static void lightLevel(PacketHandlers&, World&, io::BinaryReader&);
    static void playSoundEffect(PacketHandlers&, World&, io::BinaryReader&);
    static void mapData(PacketHandlers&, World&, io::BinaryReader&);
    static void setWeather(PacketHandlers&, World&, io::BinaryReader&);
    static void bookData(PacketHandlers&, World&, io::BinaryReader&);
    static void targetCursor(PacketHandlers&, World&, io::BinaryReader&);
    static void playMusic(PacketHandlers&, World&, io::BinaryReader&);
    static void characterAnimation(PacketHandlers&, World&, io::BinaryReader&);
    static void secureTrading(PacketHandlers&, World&, io::BinaryReader&);
    static void graphicEffect(PacketHandlers&, World&, io::BinaryReader&);
    static void bulletinBoardData(PacketHandlers&, World&, io::BinaryReader&);
    static void warmode(PacketHandlers&, World&, io::BinaryReader&);
    static void ping(PacketHandlers&, World&, io::BinaryReader&);
    static void buyList(PacketHandlers&, World&, io::BinaryReader&);
    static void updateCharacter(PacketHandlers&, World&, io::BinaryReader&);
    static void updateObject(PacketHandlers&, World&, io::BinaryReader&);
    static void openMenu(PacketHandlers&, World&, io::BinaryReader&);
    static void openPaperdoll(PacketHandlers&, World&, io::BinaryReader&);
    static void corpseEquipment(PacketHandlers&, World&, io::BinaryReader&);
    static void displayMap(PacketHandlers&, World&, io::BinaryReader&);
    static void openBook(PacketHandlers&, World&, io::BinaryReader&);
    static void dyeData(PacketHandlers&, World&, io::BinaryReader&);
    static void updateName(PacketHandlers&, World&, io::BinaryReader&);
    static void multiPlacement(PacketHandlers&, World&, io::BinaryReader&);
    static void asciiPrompt(PacketHandlers&, World&, io::BinaryReader&);
    static void sellList(PacketHandlers&, World&, io::BinaryReader&);
    static void updateHitpoints(PacketHandlers&, World&, io::BinaryReader&);
    static void updateMana(PacketHandlers&, World&, io::BinaryReader&);
    static void updateStamina(PacketHandlers&, World&, io::BinaryReader&);
    static void openUrl(PacketHandlers&, World&, io::BinaryReader&);
    static void tipWindow(PacketHandlers&, World&, io::BinaryReader&);
    static void attackCharacter(PacketHandlers&, World&, io::BinaryReader&);
    static void textEntryDialog(PacketHandlers&, World&, io::BinaryReader&);
    static void unicodeTalk(PacketHandlers&, World&, io::BinaryReader&);
    static void displayDeath(PacketHandlers&, World&, io::BinaryReader&);
    static void chatMessage(PacketHandlers&, World&, io::BinaryReader&);
    static void characterProfile(PacketHandlers&, World&, io::BinaryReader&);
    static void enableLockedFeatures(PacketHandlers&, World&, io::BinaryReader&);
    static void displayQuestArrow(PacketHandlers&, World&, io::BinaryReader&);
    static void season(PacketHandlers&, World&, io::BinaryReader&);
    static void extendedCommand(PacketHandlers&, World&, io::BinaryReader&);
    static void displayClilocString(PacketHandlers&, World&, io::BinaryReader&);
    static void unicodePrompt(PacketHandlers&, World&, io::BinaryReader&);
    static void clientViewRange(PacketHandlers&, World&, io::BinaryReader&);
    static void logout(PacketHandlers&, World&, io::BinaryReader&);
    static void megaCliloc(PacketHandlers&, World&, io::BinaryReader&);
    static void customHouse(PacketHandlers&, World&, io::BinaryReader&);
    static void oplInfo(PacketHandlers&, World&, io::BinaryReader&);
    static void buffDebuff(PacketHandlers&, World&, io::BinaryReader&);
    static void newCharacterAnimation(PacketHandlers&, World&, io::BinaryReader&);
    static void displayWaypoint(PacketHandlers&, World&, io::BinaryReader&);
    static void removeWaypoint(PacketHandlers&, World&, io::BinaryReader&);
    static void krriosClientSpecial(PacketHandlers&, World&, io::BinaryReader&);
    static void updateItemSA(PacketHandlers&, World&, io::BinaryReader&);
    static void boatMoving(PacketHandlers&, World&, io::BinaryReader&);
    static void packetList(PacketHandlers&, World&, io::BinaryReader&);

    static void partyPacket(World&, io::BinaryReader&);

    std::array<Fn, 256> _handlers{};
    std::array<Hook, 256> _hooks{};
    std::unordered_map<uint16_t, Hook> _extendedHooks;
    std::unordered_map<Serial, bool> _openBooks; // serial -> opened with 0xD4 (UTF-8 lines)
};

// Shared UpdateGameObject / UpdatePlayer / AddItemToContainer logic, used by several
// handlers and by boat movement.
void updateGameObject(World& world, Serial serial, uint16_t graphic, uint8_t graphicInc, uint16_t count, uint16_t x,
                      uint16_t y, int8_t z, Direction dir, uint16_t hue, uint8_t flagBits, uint8_t type);
void updatePlayer(World& world, Serial serial, uint16_t graphic, uint16_t hue, uint8_t flagBits, uint16_t x, uint16_t y,
                  int8_t z, Direction dir);
void addItemToContainer(World& world, Serial serial, uint16_t graphic, uint16_t amount, uint16_t x, uint16_t y,
                        uint16_t hue, Serial containerSerial);

} // namespace uo::world
