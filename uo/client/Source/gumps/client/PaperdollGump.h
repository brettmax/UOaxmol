// SPDX-License-Identifier: BSD-2-Clause
// Part of the AxmolUO client. Ported from ClassicUO Game/UI/Gumps/PaperdollGump.cs and
// Game/UI/Controls/PaperDollInteractable.cs (the classic paperdoll window).
#pragma once

#include "gumps/Controls.h"
#include "gumps/client/PaperdollOrder.h"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace uo::world
{
class World;
class Mobile;
}

namespace uo::client::gumps
{

class PaperdollEquipPic;
class PaperdollSlot;
class PaperdollButton;

// A mobile's paperdoll: body and equipment gumps in the classic client's layer order, the
// side slots for jewellery and weapons, name/title, and for the player the menu buttons.
// Double click uses an item, dragging lifts it (own paperdoll, or canLift), and dropping a
// wearable on the paperdoll equips it. Refreshes itself from the world every frame.
//
// Open it on 0x88 (WorldListener::onOpenPaperdoll).
class PaperdollGump : public Gump
{
public:
    // Actions outside the gump layer. Set once at startup; every hook is optional and the
    // gump falls back to the server request where ClassicUO has one.
    struct Hooks
    {
        std::function<void()> openOptions;                 // Options button (GameActions.OpenSettings)
        std::function<void()> logOut;                      // Log Out button (GameScene.RequestQuitGame)
        std::function<void()> openJournal;                 // Journal button (clients before 5.0.0a)
        std::function<void(uint32_t player)> openSkills;   // Skills; default requests 0x34 type 5
        std::function<void(uint32_t player)> openStatus;   // Status on own doll; default requests status
        std::function<void(uint32_t mobile)> openHealthBar;  // Status on another's doll
        std::function<void()> openParty;                   // party manifest scroll
        std::function<void()> openAbilitiesBook;           // combat book
        std::function<void()> openRacialAbilitiesBook;     // racial abilities book
        // World.ClientFeatures.PaperdollBooks (character list flags): show the books.
        bool paperdollBooks = false;
    };
    static Hooks& hooks();

    PaperdollGump(GumpContext& ctx, uo::world::World& world, uint32_t serial, bool canLift = false);

    uo::world::World& world() const { return _world; }
    bool isOwn() const;
    // The server lets the player lift items from this paperdoll (0x88 flag 0x02).
    bool canLift() const { return _canLift; }
    void setCanLift(bool v) { _canLift = v; }

    bool isMinimized() const { return _minimized; }
    void setMinimized(bool minimized);

    void refresh() override;
    bool onDropItem(const HeldItem& item, Control* target, const ax::Vec2& local) override;

private:
    enum class Button : int
    {
        Help,
        Options,
        LogOut,
        Journal,
        Quests,
        Skills,
        Guild,
        PeaceWarToggle,
        Status,
    };

    struct Shown
    {
        uint32_t serial = 0;
        uint16_t gump = 0;
        uint16_t hue = 0;
        bool partial = false;
        uint8_t layer = 0;
        bool canLift = false;
        int x = 0;

        bool operator==(const Shown&) const = default;
    };

    void build();
    PaperdollButton* addButton(Button id, uint16_t normal, uint16_t pressed, uint16_t over, int row);
    void onMenuButton(Button id);
    void syncDoll();
    void collectDoll(std::vector<Shown>& out);
    void syncSlots();
    void updateWarButton();
    uint16_t equipGumpId(uint16_t mobileGraphic, uint16_t animId, bool female) const;
    uint32_t heldSerial();

    uo::world::World& _world;
    bool _canLift;
    bool _minimized = false;
    bool _isWarMode = false;

    GumpPic* _base = nullptr;
    PaperdollButton* _warButton = nullptr;
    TextLabel* _title = nullptr;
    std::string _titleText;
    Control* _dollLayer = nullptr;
    std::vector<PaperdollSlot*> _slots;
    // Doll pictures are reused, never destroyed while the gump lives: the manager keeps raw
    // pointers to the pressed and hovered controls.
    std::vector<PaperdollEquipPic*> _dollPics;
    std::vector<Shown> _shown;
    std::vector<Shown> _scratch;
    bool _shownValid = false;
};

// A body or equipment gump on the doll (GumpPicEquipment).
class PaperdollEquipPic : public GumpPic
{
public:
    explicit PaperdollEquipPic(PaperdollGump& owner);

    uint32_t serial() const { return _serial; }
    uo::world::Layer layer() const { return _layer; }
    void assign(uint32_t serial, uint16_t gump, uint16_t hue, bool partial, uo::world::Layer layer, bool canLift);
    void clear();

    void onClick(MouseButton button) override;
    void onDoubleClick(MouseButton button) override;
    bool beginDrag() override;
    bool containsLocal(const ax::Vec2& local) const override;

private:
    PaperdollGump& _owner;
    uint32_t _serial = 0;
    uo::world::Layer _layer = uo::world::Layer::Invalid;
    bool _canLift = false;
    bool _active = false;
};

// One of the side slots (EquipmentSlot): a framed 19x20 cell showing the item on a layer.
class PaperdollSlot : public Control
{
public:
    PaperdollSlot(PaperdollGump& owner, uo::world::Layer layer);

    uo::world::Layer layer() const { return _layer; }
    uint32_t serial() const { return _serial; }
    // Shows the item (0 for none).
    void setItem(uint32_t serial, uint16_t graphic, uint16_t hue, bool canLift);

    void onClick(MouseButton button) override;
    void onDoubleClick(MouseButton button) override;
    bool beginDrag() override;

private:
    PaperdollGump& _owner;
    uo::world::Layer _layer;
    uint32_t _serial = 0;
    uint16_t _graphic = 0;
    uint16_t _hue = 0;
    bool _canLift = false;
    ax::Node* _art = nullptr;
};

// A GumpButton whose art can change (the peace / war toggle).
class PaperdollButton : public GumpButton
{
public:
    PaperdollButton(GumpContext& ctx, uint16_t normal, uint16_t pressed, uint16_t over)
        : GumpButton(ctx, normal, pressed, over)
    {
    }

    void setGraphics(uint16_t normal, uint16_t pressed, uint16_t over)
    {
        _normal = normal;
        _pressed = pressed;
        _over = over;
        showState();
    }
};

}  // namespace uo::client::gumps
