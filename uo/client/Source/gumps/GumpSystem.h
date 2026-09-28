// SPDX-License-Identifier: BSD-2-Clause
// Part of the AxmolUO client. Wires the gump layer into the client: owns the services and
// the GumpManager, takes 0xB0 / 0xDD from uo::world::PacketHandlers, and opens the client
// gumps from world events. GameClient forwards the WorldListener calls listed below.
#pragma once

#include "gumps/GumpActions.h"
#include "gumps/GumpManager.h"
#include "gumps/client/ContainerData.h"

#include <functional>
#include <memory>
#include <vector>

namespace uo::assets
{
class Installation;
}

namespace uo::world
{
class Entity;
class Item;
class Mobile;
class PacketHandlers;
class World;
}  // namespace uo::world

namespace uo::client::gumps
{

class GumpSystem
{
public:
    // `send` sends a whole packet to the server.
    GumpSystem(const uo::assets::Installation& assets, uo::world::World& world,
               std::function<void(std::vector<uint8_t>)> send);
    ~GumpSystem();

    // Registers the 0xB0 and 0xDD handlers.
    void install(uo::world::PacketHandlers& handlers);

    // Creates the manager node for a scene (the world scene adds it above the game view).
    GumpManager* createManager();
    GumpManager* manager() const { return _manager.get(); }

    GumpContext& context() { return _ctx; }
    GumpActions& actions() { return *_actions; }

    // Replace the text fallback with the UO font renderer when it is available.
    void setText(GumpText* text) { _ctx.text = text ? text : _fallbackText.get(); }

    // WorldListener forwards.
    void onOpenContainer(uo::world::Item& container, uint16_t gumpGraphic);
    void onOpenPaperdoll(uo::world::Mobile& mobile, const std::string& title, bool canLift);
    void onCloseServerGump(uint32_t gumpId, uint32_t button);
    void onStatsChanged(uo::world::Entity& entity);
    void onSkillsChanged(int skillIndex, bool openWindow);
    void onEntityRemoved(uint32_t serial);
    void onContainerContentsChanged(uint32_t container);
    void onEquipmentChanged(uint32_t mobile);
    void onNameChanged(uo::world::Entity& entity);
    // CloseUiKind::Paperdoll (1) or Status (2), from 0xBF 0x16.
    void onCloseUi(uint32_t kind, uint32_t serial);
    // Drag outcome (0x27 reject, 0x28 / 0x29): the held item leaves the cursor.
    void onDragEnded();

    void openStatus(uint32_t serial);
    void openSkills();
    void openHealthBar(uint32_t serial);

private:
    uo::world::World& _world;
    std::unique_ptr<GumpTextures> _textures;
    std::unique_ptr<GumpText> _fallbackText;
    std::unique_ptr<GumpActions> _actions;
    GumpContext _ctx;
    ax::RefPtr<GumpManager> _manager;
    ContainerPlacement _containerPlacement;

    ax::Vec2 nextContainerPosition(const ax::Size& gumpSize);
};

}  // namespace uo::client::gumps
