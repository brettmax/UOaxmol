// SPDX-License-Identifier: BSD-2-Clause
// Part of the AxmolUO client. Owns the open gumps and routes the mouse to them. Ported from
// ClassicUO.Game.Managers.UIManager (gump list, saved server gump positions, mouse capture,
// dragging, tooltips) and the gump side of 0xB0 / 0xDD / 0xBF.04.
#pragma once

#include "gumps/ServerGump.h"

#include <optional>
#include <span>
#include <unordered_map>

namespace uo::client::gumps
{

// A full-screen node above the world. Add it to the scene once; it claims the pointer only
// when the pointer is over a gump, so the world still gets clicks everywhere else.
class GumpManager : public ax::Node
{
public:
    static GumpManager* create(GumpContext& ctx);

    GumpContext& context() const { return _ctx; }

    // Adds a gump at a UO screen position (top-left, y down) and brings it to the front.
    void add(Gump* gump, const ax::Vec2& screenPos);
    void bringToFront(Gump* gump);

    // First open gump of `kind` for `serial` (and `serverId` for server gumps).
    Gump* find(GumpKind kind, uint32_t serial, uint32_t serverId = 0) const;
    template <typename T>
    T* find(uint32_t serial) const
    {
        for (auto* g : _gumps)
        {
            if (auto* t = dynamic_cast<T*>(g); t && !g->isClosed() && g->serial() == serial)
            {
                return t;
            }
        }

        return nullptr;
    }
    const std::vector<Gump*>& gumps() const { return _gumps; }

    void closeAll(GumpKind kind);

    // 0xB0 / 0xDD: open (or rebuild in place) a server gump. Returns false for a packet
    // that is not a gump or does not decode.
    bool handleGumpPacket(std::span<const uint8_t> packet);
    ServerGump* openServerGump(const uo::gumps::GumpLayout& layout);
    // 0xBF sub 0x04: the server closes a gump, optionally as if a button was pressed.
    void closeServerGump(uint32_t gumpId, uint32_t buttonId);

    // Server gump positions remembered per gump type id (UIManager.SavePosition).
    void savePosition(uint32_t gumpId, const ax::Vec2& pos) { _savedPositions[gumpId] = pos; }
    void removePosition(uint32_t gumpId) { _savedPositions.erase(gumpId); }

    // The item hanging on the cursor, if any.
    const std::optional<HeldItem>& heldItem() const { return _held; }
    void setHeldItem(std::optional<HeldItem> item);
    // Released over no gump: the world decides (drop on ground, on a mobile, ...).
    std::function<void(const HeldItem&, const ax::Vec2& screen)> onDropOnWorld;

    // itemproperty tooltips: returns the property text for a serial (empty while unknown).
    std::function<std::string(uint32_t serial)> propertyText;

    // Screen (UO, y down) <-> world (Axmol, y up).
    ax::Vec2 toScreen(const ax::Vec2& world) const;
    ax::Vec2 toWorld(const ax::Vec2& screen) const;

    // Whether the pointer is over any gump (the world should ignore it then).
    bool isOverGump(const ax::Vec2& world) const;

    void update(float dt) override;
    void onEnter() override;
    void onExit() override;

private:
    explicit GumpManager(GumpContext& ctx);

    bool onPointerDown(ax::PointerEvent* e);
    void onPointerMove(ax::PointerEvent* e);
    void onPointerUp(ax::PointerEvent* e);
    void onPointerScroll(ax::PointerEvent* e);
    void onKeyPressed(ax::KeyboardEvent* e);

    Gump* gumpAt(const ax::Vec2& world) const;
    void prune();
    void setHover(Control* c);
    void showTooltip(const std::string& text, const ax::Vec2& world);
    void hideTooltip();
    void keepOnScreen(Gump* g);

    GumpContext& _ctx;
    std::vector<Gump*> _gumps;  // back to front; retained
    std::unordered_map<uint32_t, ax::Vec2> _savedPositions;

    // Mouse capture.
    Gump* _pressedGump = nullptr;
    // Controls are retained while referenced here: a gump can drop a control (rebuilding a
    // list, swapping art) while the pointer is still on it.
    ax::RefPtr<Control> _pressedControl;
    MouseButton _pressedButton = MouseButton::Left;
    ax::Vec2 _pressWorld;
    ax::Vec2 _gumpStart;
    bool _dragging = false;
    bool _dragMovesGump = false;
    ax::RefPtr<Control> _hover;

    // Double click.
    ax::RefPtr<Control> _lastClickControl;
    float _lastClickTime = -1;
    float _clock = 0;

    // Tooltips.
    float _hoverTime = 0;
    ax::Node* _tooltip = nullptr;
    ax::Vec2 _pointerWorld;

    std::optional<HeldItem> _held;
    ax::Sprite* _heldSprite = nullptr;

    ax::PointerEventListener* _pointer = nullptr;
    ax::EventListenerKeyboard* _keyboard = nullptr;
};

}  // namespace uo::client::gumps
