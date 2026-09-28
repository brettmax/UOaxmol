// SPDX-License-Identifier: BSD-2-Clause
// Part of the AxmolUO client. Ported from ClassicUO Game/UI/Gumps/ContainerGump.cs and
// Game/UI/Controls/ItemGump.cs (the classic container window, not the grid container).
#pragma once

#include "gumps/Controls.h"
#include "gumps/client/ContainerData.h"

#include <cstdint>
#include <vector>

namespace uo::world
{
class World;
}

namespace uo::client::gumps
{

class ContainerItemPic;

// A container window: the container's gump with each item drawn at its x/y. Double click
// uses an item, a click shows its name, dragging lifts it, and dropping the held item places
// it where it was released. Refreshes itself from the world every frame.
//
// Open it on 0x24 (WorldListener::onOpenContainer) with the gump id from the packet, at the
// position ContainerPlacement gives, or in place of an already open gump for the same serial.
class ContainerGump : public Gump
{
public:
    ContainerGump(GumpContext& ctx, uo::world::World& world, uint32_t serial, uint16_t gumpGraphic,
                  bool playOpenSound = true);

    uint16_t graphic() const { return _graphic; }
    const ContainerData& data() const { return _data; }
    uo::world::World& world() const { return _world; }

    bool isChessboard() const { return _graphic == kChessboardGump; }
    bool isBackgammonBoard() const { return _graphic == kBackgammonGump; }
    // Board games draw their pieces from gumps instead of item art.
    bool itemsAreGumps() const { return isChessboard() || isBackgammonBoard(); }

    bool isMinimized() const { return _minimized; }
    void setMinimized(bool minimized);

    void refresh() override;
    bool onDropItem(const HeldItem& item, Control* target, const ax::Vec2& local) override;
    // Right click: plays the close sound and closes the containers opened from this one.
    void onCloseRequested() override;

    // Closes this gump and every open container gump for items inside it (ContainerGump.Dispose).
    void closeWithChildren();

private:
    struct Shown
    {
        uint32_t serial = 0;
        uint16_t graphic = 0;
        uint16_t hue = 0;
        int x = 0;
        int y = 0;
        bool stacked = false;

        bool operator==(const Shown&) const = default;
    };

    void build();
    void syncItems();
    void collectItems(std::vector<Shown>& out);
    // The held item's graphic without the coin pile offset (ItemHold.Graphic).
    uint16_t heldBaseGraphic(const HeldItem& item) const;

    uo::world::World& _world;
    uint16_t _graphic;
    ContainerData _data;
    bool _minimized = false;

    GumpPic* _background = nullptr;
    Control* _minimizer = nullptr;
    GumpPic* _eye = nullptr;
    Control* _itemsLayer = nullptr;
    // Item controls are reused, never destroyed while the gump lives: the manager keeps raw
    // pointers to the pressed and hovered controls.
    std::vector<ContainerItemPic*> _itemPics;
    std::vector<Shown> _shown;
    std::vector<Shown> _scratch;
    bool _shownValid = false;

    float _eyeClock = 0;
    int _eyeFrame = 0;
};

// One item inside a container gump (ItemGump): art (or a gump for board pieces), hued,
// highlighted under the mouse, drawn twice for a stack.
class ContainerItemPic : public Control
{
public:
    explicit ContainerItemPic(ContainerGump& owner);

    uint32_t serial() const { return _serial; }
    void assign(uint32_t serial, uint16_t graphic, uint16_t hue, bool stacked);
    // Returns the control to the pool: hidden and inert.
    void clear();

    bool containsLocal(const ax::Vec2& local) const override;
    void onClick(MouseButton button) override;
    void onDoubleClick(MouseButton button) override;
    void onHover(bool entered) override;
    bool beginDrag() override;

private:
    void rebuildSprites();
    bool opaqueAt(int x, int y) const;

    ContainerGump& _owner;
    uint32_t _serial = 0;
    uint16_t _graphic = 0;
    uint16_t _hue = 0;
    bool _stacked = false;
    bool _hovered = false;
};

}  // namespace uo::client::gumps
