// SPDX-License-Identifier: BSD-2-Clause
// Part of the AxmolUO client. Ported from ClassicUO.Game.UI (Controls/Control, Gumps/Gump).
//
// Coordinates: UO lays gumps out from the top-left with y growing down; Axmol's origin is
// bottom-left with y up. Every gump node sits at its top-left corner and has a zero content
// size, and every control is anchored at its own top-left (anchor 0,1). A child at UO local
// (x, y) inside a parent of height h is therefore positioned at (x, h - y); for children of
// the gump root, h is 0. placeTopLeft does exactly that, and toLocal goes the other way.
#pragma once

#include "gumps/GumpServices.h"

#include "axmol/axmol.h"

#include <cstdint>
#include <string>
#include <vector>

namespace uo::client::gumps
{

class Gump;

void placeTopLeft(ax::Node* child, float x, float y, float parentHeight);

enum class MouseButton : int
{
    Left = ax::InputButton::Left,
    Right = ax::InputButton::Right,
    Middle = ax::InputButton::Middle,
};

// Base of every gump element: a node with UO-style placement, a page, hit testing and
// mouse callbacks. The GumpManager routes pointer input to it.
class Control : public ax::Node
{
public:
    // Page 0 is drawn on every page of its gump.
    int page() const { return _page; }
    void setPage(int page) { _page = page; }

    // Whether the control takes clicks. Controls that do not are transparent to input
    // (the gump behind them can still be dragged).
    bool acceptsInput() const { return _acceptsInput; }
    void setAcceptsInput(bool v) { _acceptsInput = v; }

    // Whether a drag that starts on this control moves the gump (true for art, false for
    // buttons, checkboxes and text entries).
    bool movesGump() const { return _movesGump; }
    void setMovesGump(bool v) { _movesGump = v; }

    // Switch id (checkbox/radio) or text entry id, echoed in the 0xB1 reply.
    uint32_t localSerial() const { return _localSerial; }
    void setLocalSerial(uint32_t s) { _localSerial = s; }

    const std::string& tooltip() const { return _tooltip; }
    void setTooltip(std::string t) { _tooltip = std::move(t); }
    void appendTooltip(const std::string& line);
    // itemproperty: the tooltip is the object's property list, fetched by serial.
    uint32_t tooltipSerial() const { return _tooltipSerial; }
    void setTooltipSerial(uint32_t s) { _tooltipSerial = s; }

    // UO placement relative to the parent control or gump.
    void setUOPosition(float x, float y);
    float uoX() const { return _uoX; }
    float uoY() const { return _uoY; }
    void setUOSize(float w, float h);

    // World point to this control's UO local space (top-left origin).
    ax::Vec2 toLocal(const ax::Vec2& world) const;

    // Local point is inside. Default: the bounds; pictures override with a pixel test.
    virtual bool containsLocal(const ax::Vec2& local) const;

    virtual void onMouseDown(MouseButton, const ax::Vec2& /*local*/) {}
    // `inside` is whether the pointer is still over the control on release.
    virtual void onMouseUp(MouseButton, bool /*inside*/) {}
    virtual void onClick(MouseButton) {}
    virtual void onDoubleClick(MouseButton) {}
    virtual void onHover(bool /*entered*/) {}

    // Item drag (containers, paperdoll): return true to start a drag of this control.
    virtual bool beginDrag() { return false; }

    Gump* gump() const;

protected:
    void onUOSizeChanged();

private:
    int _page = 0;
    bool _acceptsInput = false;
    bool _movesGump = true;
    uint32_t _localSerial = 0;
    std::string _tooltip;
    uint32_t _tooltipSerial = 0;
    float _uoX = 0, _uoY = 0;
};

// An item being dragged with the mouse (lifted from a container, the paperdoll or the world).
struct HeldItem
{
    uint32_t serial = 0;
    uint16_t graphic = 0;
    uint16_t hue = 0;
    uint16_t amount = 1;
    // Where it was picked up from, so a rejected drop can put it back.
    uint32_t sourceContainer = 0;
};

// Kinds the manager uses to find, save and replace gumps.
enum class GumpKind : uint8_t
{
    Server,
    Container,
    Paperdoll,
    Status,
    Skills,
    Other,
};

// A window: a root node at the gump's top-left plus its controls. Handles pages, closing
// rules and the reply for server gumps (see ServerGump).
class Gump : public ax::Node
{
public:
    Gump(GumpContext& ctx, GumpKind kind, uint32_t serial, uint32_t serverId);

    GumpKind kind() const { return _kind; }
    // For server gumps: the sender serial and the gump type id of 0xB0/0xDD. For client
    // gumps: the object serial (container, mobile) and 0.
    uint32_t serial() const { return _serial; }
    uint32_t serverId() const { return _serverId; }

    GumpContext& context() const { return _ctx; }

    bool canMove() const { return _canMove; }
    void setCanMove(bool v) { _canMove = v; }
    bool canCloseWithRightClick() const { return _canCloseWithRightClick; }
    void setCanCloseWithRightClick(bool v) { _canCloseWithRightClick = v; }
    bool canCloseWithEsc() const { return _canCloseWithEsc; }
    void setCanCloseWithEsc(bool v) { _canCloseWithEsc = v; }

    // Top-left corner in UO screen coordinates (y down); the manager converts.
    ax::Vec2 screenPosition() const { return _screen; }
    void setScreenPosition(const ax::Vec2& p);

    template <typename T>
    T* add(T* control, int page = 0)
    {
        addControl(control, page);
        return control;
    }
    void addControl(Control* control, int page);
    const std::vector<Control*>& controls() const { return _controls; }
    void clearControls();

    int activePage() const { return _activePage; }
    virtual void setActivePage(int page);

    // Bounding box of the visible controls in UO local coordinates.
    ax::Rect uoBounds() const;

    // Deepest input-accepting control under a world point on the active page, or nullptr.
    Control* controlAt(const ax::Vec2& world) const;
    // Any visible control under the point (for "is the pointer over this gump").
    bool containsWorld(const ax::Vec2& world) const;

    // A button with an activate action was clicked.
    virtual void onButton(uint32_t buttonId) {}
    // Right-click close (when allowed). Server gumps reply with button 0 here.
    virtual void onCloseRequested() { close(); }
    // Keyboard: Escape. ClassicUO tracks `nodispose` but never closes gumps on Escape, so the
    // default does nothing; client gumps may override.
    virtual void onEscape() {}

    // Removes the gump from the manager and the scene.
    void close();
    bool isClosed() const { return _closed; }

    // An item was released over this gump. `target` is the control under the pointer (may be
    // null), `local` the point in gump UO coordinates. Return true if the gump took it.
    virtual bool onDropItem(const HeldItem& /*item*/, Control* /*target*/, const ax::Vec2& /*local*/) { return false; }

    // Mouse wheel over the gump; return true if consumed.
    virtual bool onWheel(Control* /*target*/, float /*dy*/) { return false; }

    // Called by the manager once per frame; client gumps refresh from the world here.
    virtual void refresh() {}

protected:
    void applyPageVisibility();

private:
    Control* hitTest(Control* c, const ax::Vec2& world, bool inputOnly) const;

    GumpContext& _ctx;
    GumpKind _kind;
    uint32_t _serial;
    uint32_t _serverId;
    bool _canMove = true;
    bool _canCloseWithRightClick = true;
    bool _canCloseWithEsc = true;
    bool _closed = false;
    int _activePage = 1;
    ax::Vec2 _screen;
    std::vector<Control*> _controls;
};

}  // namespace uo::client::gumps
