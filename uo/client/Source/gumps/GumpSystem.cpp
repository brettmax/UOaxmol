// SPDX-License-Identifier: BSD-2-Clause
#include "gumps/GumpSystem.h"

#include "gumps/client/ContainerGump.h"
#include "gumps/client/ContainerItemSupport.h"
#include "gumps/client/PaperdollGump.h"
#include "gumps/client/SkillsGump.h"
#include "gumps/client/StatusGump.h"

#include "uo/assets/Installation.h"
#include "uo/io/BinaryReader.h"
#include "uo/world/PacketHandlers.h"
#include "uo/world/World.h"

namespace uo::client::gumps
{

namespace
{
// Where the classic client puts the gumps it opens without a saved position.
const ax::Vec2 kPaperdollPosition(100, 100);
const ax::Vec2 kStatusPosition(100, 100);
const ax::Vec2 kSkillsPosition(100, 100);

// There is one skills window, whichever player it was opened for.
SkillsGump* findSkills(const GumpManager& manager)
{
    for (auto* g : manager.gumps())
    {
        if (auto* s = dynamic_cast<SkillsGump*>(g); s && !g->isClosed())
        {
            return s;
        }
    }

    return nullptr;
}
}  // namespace

GumpSystem::GumpSystem(const uo::assets::Installation& assets, uo::world::World& world,
                       std::function<void(std::vector<uint8_t>)> send)
    : _world(world),
      _textures(std::make_unique<AssetGumpTextures>(assets)),
      _fallbackText(std::make_unique<FallbackGumpText>(&assets)),
      _actions(std::make_unique<PacketGumpActions>(send))
{
    _ctx.textures = _textures.get();
    _ctx.text = _fallbackText.get();
    _ctx.actions = _actions.get();
    _ctx.send = std::move(send);

    setItemTileData(&assets.tileData());

    auto& hooks = PaperdollGump::hooks();
    hooks.openStatus = [this](uint32_t serial) { openStatus(serial); };
    hooks.openHealthBar = [this](uint32_t serial) { openHealthBar(serial); };
    // Without this the Skills button only sent 0x34 type 5, and the server's reply (0x3A
    // type 0 on pre-AOS shards) never opens a window: World::skillsRequested is not set.
    hooks.openSkills = [this](uint32_t) { openSkills(); };
}

GumpSystem::~GumpSystem()
{
    // The manager node can outlive us in a scene that stays up across a world reset; take it
    // out so its listeners and gumps stop before the context they reference goes away.
    if (_manager)
    {
        _manager->removeFromParent();
        _manager = nullptr;
    }

    setItemTileData(nullptr);
    PaperdollGump::hooks().openStatus = nullptr;
    PaperdollGump::hooks().openHealthBar = nullptr;
    PaperdollGump::hooks().openSkills = nullptr;
}

void GumpSystem::install(uo::world::PacketHandlers& handlers)
{
    auto forward = [this](uo::world::World&, io::BinaryReader& r) {
        if (_manager)
        {
            _manager->handleGumpPacket(r.buffer());
        }
    };

    handlers.setHook(0xB0, forward);
    handlers.setHook(0xDD, forward);
}

GumpManager* GumpSystem::createManager()
{
    if (_manager)
    {
        _manager->closeAll(GumpKind::Server);
        _manager->removeFromParent();
    }

    _manager = GumpManager::create(_ctx);
    return _manager.get();
}

ax::Vec2 GumpSystem::nextContainerPosition(const ax::Size& gumpSize)
{
    auto screen = ax::Director::getInstance()->getCanvasSize();
    int x = 0;
    int y = 0;
    _containerPlacement.next(static_cast<int>(gumpSize.width), static_cast<int>(gumpSize.height),
                             static_cast<int>(screen.width), static_cast<int>(screen.height), x, y);
    return ax::Vec2(static_cast<float>(x), static_cast<float>(y));
}

void GumpSystem::onOpenContainer(uo::world::Item& container, uint16_t gumpGraphic)
{
    // 0xFFFF is a spellbook and 0x0030 a vendor window; neither is a container gump.
    if (!_manager || gumpGraphic == 0xFFFF || gumpGraphic == 0x0030)
    {
        return;
    }

    // Re-opening an open container replaces it in place, as ClassicUO does.
    ax::Vec2 pos;
    bool known = false;

    if (auto* old = _manager->find<ContainerGump>(container.serial))
    {
        pos = old->screenPosition();
        known = true;
        old->close();
    }

    auto* gump = new ContainerGump(_ctx, _world, container.serial, gumpGraphic, !known);
    if (!known)
    {
        auto bounds = gump->uoBounds();
        pos = nextContainerPosition(ax::Size(bounds.getMaxX(), bounds.getMaxY()));
    }

    _manager->add(gump, pos);
}

void GumpSystem::onOpenPaperdoll(uo::world::Mobile& mobile, const std::string& /*title*/, bool canLift)
{
    if (!_manager)
    {
        return;
    }

    // The title is already on the mobile; the gump reads it from there when it refreshes.
    if (auto* old = _manager->find<PaperdollGump>(mobile.serial))
    {
        old->setCanLift(canLift);
        old->refresh();
        _manager->bringToFront(old);
        return;
    }

    _manager->add(new PaperdollGump(_ctx, _world, mobile.serial, canLift), kPaperdollPosition);
}

void GumpSystem::onCloseServerGump(uint32_t gumpId, uint32_t button)
{
    if (_manager)
    {
        _manager->closeServerGump(gumpId, button);
    }
}

void GumpSystem::onStatsChanged(uo::world::Entity& entity)
{
    if (!_manager)
    {
        return;
    }

    if (auto* g = _manager->find<StatusGump>(entity.serial))
    {
        g->refresh();
    }

    if (auto* g = _manager->find<StatusBarGump>(entity.serial))
    {
        g->refresh();
    }
}

void GumpSystem::onSkillsChanged(int /*skillIndex*/, bool openWindow)
{
    if (!_manager)
    {
        return;
    }

    if (auto* g = findSkills(*_manager))
    {
        g->refresh();
        if (openWindow)
        {
            _manager->bringToFront(g);
        }
    }
    else if (openWindow)
    {
        openSkills();
    }
}

void GumpSystem::onEntityRemoved(uint32_t serial)
{
    if (!_manager)
    {
        return;
    }

    if (auto* g = _manager->find<ContainerGump>(serial))
    {
        g->closeWithChildren();
    }

    if (auto* g = _manager->find<PaperdollGump>(serial))
    {
        g->close();
    }
}

void GumpSystem::onContainerContentsChanged(uint32_t container)
{
    if (_manager)
    {
        if (auto* g = _manager->find<ContainerGump>(container))
        {
            g->refresh();
        }
    }
}

void GumpSystem::onEquipmentChanged(uint32_t mobile)
{
    if (_manager)
    {
        if (auto* g = _manager->find<PaperdollGump>(mobile))
        {
            g->refresh();
        }
    }
}

void GumpSystem::onNameChanged(uo::world::Entity& entity)
{
    onStatsChanged(entity);
    onEquipmentChanged(entity.serial);
}

void GumpSystem::onCloseUi(uint32_t kind, uint32_t serial)
{
    if (!_manager)
    {
        return;
    }

    if (kind == static_cast<uint32_t>(uo::world::CloseUiKind::Paperdoll))
    {
        if (auto* g = _manager->find<PaperdollGump>(serial))
        {
            g->close();
        }
    }
    else if (kind == static_cast<uint32_t>(uo::world::CloseUiKind::Status))
    {
        if (auto* g = _manager->find<StatusGump>(serial))
        {
            g->close();
        }

        if (auto* g = _manager->find<StatusBarGump>(serial))
        {
            g->close();
        }
    }
}

void GumpSystem::onDragEnded()
{
    if (_manager)
    {
        _manager->setHeldItem(std::nullopt);
    }
}

void GumpSystem::openStatus(uint32_t serial)
{
    if (!_manager)
    {
        return;
    }

    if (auto* g = _manager->find<StatusGump>(serial))
    {
        _manager->bringToFront(g);
        return;
    }

    if (auto* bar = _manager->find<StatusBarGump>(serial))
    {
        _manager->bringToFront(bar);
        return;
    }

    // Only the player's own status has a full gump in the classic client.
    auto* player = _world.player();
    if (!player || player->serial != serial)
    {
        return;
    }

    _actions->requestStatus(serial);
    _manager->add(new StatusGump(_ctx, _world), kStatusPosition);
}

void GumpSystem::openHealthBar(uint32_t serial)
{
    if (!_manager)
    {
        return;
    }

    if (auto* bar = _manager->find<StatusBarGump>(serial))
    {
        _manager->bringToFront(bar);
        return;
    }

    // Health bars for other mobiles belong to the world layer (drag-off bars); until that
    // lands, only the player's own bar opens here, and others just refresh their stats.
    _actions->requestStatus(serial);
}

void GumpSystem::openSkills()
{
    if (!_manager)
    {
        return;
    }

    if (auto* g = findSkills(*_manager))
    {
        _manager->bringToFront(g);
        return;
    }

    _manager->add(new SkillsGump(_ctx, _world), kSkillsPosition);
}

}  // namespace uo::client::gumps
