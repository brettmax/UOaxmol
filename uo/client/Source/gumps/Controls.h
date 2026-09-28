// SPDX-License-Identifier: BSD-2-Clause
// Part of the AxmolUO client. The gump controls, ported from ClassicUO.Game.UI.Controls:
// GumpPic, GumpPicTiled, ResizePic, Button, ButtonTileArt, Checkbox, RadioButton, StaticPic,
// GumpPicInPic, Label/CroppedText, HtmlControl, StbTextBox and CheckerTrans.
#pragma once

#include "gumps/Gump.h"

#include "axmol/ui/InputField.h"

#include <functional>

namespace uo::client::gumps
{

// Fills a rectangle with a texture repeated from its top-left; the last row and column are
// cropped. Built from sprites so it works whether or not the texture is in an atlas.
class TiledTexture : public ax::Node
{
public:
    static TiledTexture* create(ax::Texture2D* texture, float width, float height);
    void setColorAll(const ax::Color32& c);
    void setOpacityAll(uint8_t alpha);
};

// A gump image. Hit testing is per pixel, like ClassicUO's GumpPic.Contains.
class GumpPic : public Control
{
public:
    GumpPic(GumpContext& ctx, uint16_t graphic, uint16_t hue = 0, bool partialHue = false);

    uint16_t graphic() const { return _graphic; }
    void setGraphic(uint16_t graphic, uint16_t hue = 0, bool partialHue = false);
    // Hit the whole rectangle instead of opaque pixels (virtue gumps, some buttons).
    void setContainsByBounds(bool v) { _byBounds = v; }

    bool containsLocal(const ax::Vec2& local) const override;
    ax::Sprite* sprite() const { return _sprite; }

private:
    GumpContext& _ctx;
    uint16_t _graphic = 0;
    bool _byBounds = false;
    ax::Sprite* _sprite = nullptr;
};

// A gump image tiled over a rectangle (gumppictiled).
class GumpPicTiled : public Control
{
public:
    GumpPicTiled(GumpContext& ctx, uint16_t graphic, float width, float height, uint16_t hue = 0);
};

// A nine-part background: graphic + 0..8 (corners, tiled edges, tiled centre).
class ResizePic : public Control
{
public:
    ResizePic(GumpContext& ctx, uint16_t graphic, float width, float height);

    uint16_t graphic() const { return _graphic; }
    void resize(float width, float height);

private:
    void build();

    GumpContext& _ctx;
    uint16_t _graphic;
};

// A gump button: normal, pressed and optional hover art. Activates (sends the button id)
// or switches page.
class GumpButton : public Control
{
public:
    GumpButton(GumpContext& ctx, uint16_t normal, uint16_t pressed, uint16_t over = 0);

    uint32_t buttonId() const { return _buttonId; }
    void setButtonId(uint32_t id) { _buttonId = id; }
    bool activates() const { return _activates; }
    void setActivates(bool v) { _activates = v; }
    int toPage() const { return _toPage; }
    void setToPage(int page) { _toPage = page; }
    // Swaps the art, e.g. a lock or expand button changing state.
    void setGraphics(uint16_t normal, uint16_t pressed, uint16_t over = 0);

    // Client gumps can handle clicks directly instead of going through Gump::onButton.
    std::function<void(GumpButton*)> onClicked;

    void onMouseDown(MouseButton, const ax::Vec2&) override;
    void onMouseUp(MouseButton, bool inside) override;
    void onClick(MouseButton button) override;
    void onHover(bool entered) override;
    bool containsLocal(const ax::Vec2& local) const override;

protected:
    void showState();

    GumpContext& _ctx;
    uint16_t _normal, _pressed, _over;
    uint32_t _buttonId = 0;
    bool _activates = true;
    int _toPage = 0;
    bool _isPressed = false;
    bool _isHovered = false;
    ax::Sprite* _sprite = nullptr;
    ax::Node* _placeholder = nullptr;  // stands in for art the client files lack
};

// buttontileart: a button with a piece of item art centred in a cell of the given size;
// the whole cell is clickable (ClassicUO's ButtonTileArt, as fixed in ModernUO-Client).
class ButtonTileArt : public GumpButton
{
public:
    ButtonTileArt(GumpContext& ctx, uint16_t normal, uint16_t pressed, uint16_t tile, uint16_t tileHue,
                  float cellWidth, float cellHeight);

    bool containsLocal(const ax::Vec2& local) const override;
};

// Checkbox and radio button (radio: checking one unchecks the rest of its group).
class Checkbox : public Control
{
public:
    Checkbox(GumpContext& ctx, uint16_t unchecked, uint16_t checked, bool isChecked, bool isRadio = false,
             int group = 0);

    bool isChecked() const { return _checked; }
    void setChecked(bool v);
    bool isRadio() const { return _radio; }
    int group() const { return _group; }

    std::function<void(Checkbox*)> onChanged;

    void onClick(MouseButton button) override;

private:
    GumpContext& _ctx;
    uint16_t _uncheckedGraphic, _checkedGraphic;
    bool _checked;
    bool _radio;
    int _group;
    ax::Sprite* _sprite = nullptr;
    ax::Node* _placeholder = nullptr;  // stands in for art the client files lack
};

// Item art (tilepic / tilepichue), hued, with the tiledata partial-hue flag.
class StaticPic : public Control
{
public:
    StaticPic(GumpContext& ctx, uint16_t graphic, uint16_t hue = 0);

    uint16_t graphic() const { return _graphic; }
    void setGraphic(uint16_t graphic, uint16_t hue);
    bool containsLocal(const ax::Vec2& local) const override;

private:
    GumpContext& _ctx;
    uint16_t _graphic = 0;
    ax::Sprite* _sprite = nullptr;
};

// picinpic: a sub-rectangle of a gump.
class GumpPicInPic : public Control
{
public:
    GumpPicInPic(GumpContext& ctx, uint16_t graphic, float sx, float sy, float width, float height,
                 uint16_t hue = 0, bool partialHue = false);
};

// text / croppedtext: a single label from the gump's text table.
class TextLabel : public Control
{
public:
    TextLabel(GumpContext& ctx, std::string_view text, const GumpTextStyle& style, float cropWidth = 0,
              float cropHeight = 0);

    void setText(std::string_view text);
    const std::string& text() const { return _text; }

private:
    GumpContext& _ctx;
    GumpTextStyle _style;
    std::string _text;
    ax::Node* _label = nullptr;
    float _cropW, _cropH;
};

// htmlgump / xmfhtml*: HTML text in a box, optionally with the gump background (3000) and
// a scrollbar. Wheel scrolls; the scrollbar is drawn from the classic scroll gumps.
class HtmlArea : public Control
{
public:
    HtmlArea(GumpContext& ctx, std::string_view html, float width, float height, bool background,
             int scrollStyle /*0 none, 1 bar, 2 flag*/, uint32_t color /*wire colour, 0 = default*/);

    void scrollBy(float dy);
    bool containsLocal(const ax::Vec2& local) const override;
    void onMouseDown(MouseButton, const ax::Vec2& local) override;

private:
    void updateScroll();

    GumpContext& _ctx;
    ax::Node* _content = nullptr;
    ax::ClippingRectangleNode* _clip = nullptr;
    ax::Sprite* _thumb = nullptr;
    float _scroll = 0;
    float _maxScroll = 0;
    float _viewH = 0;
    bool _hasScrollbar = false;
};

// textentry / textentrylimited.
class TextEntry : public Control
{
public:
    TextEntry(GumpContext& ctx, float width, float height, uint16_t hue, std::string_view text, int maxLength);

    std::string text() const;
    void focus();
    void onClick(MouseButton button) override;

private:
    // Redraws the UO-font text and caret when the input's text or cursor moved.
    void sync(bool force = false);

    GumpContext& _ctx;
    GumpTextStyle _style;
    // Takes keyboard and IME input; its own TTF text and cursor are transparent, and the
    // text is drawn with the UO unicode font instead, as ClassicUO's StbTextBox does.
    class Input;
    Input* _field = nullptr;
    ax::Node* _clip = nullptr;
    ax::Node* _label = nullptr;
    ax::Node* _caret = nullptr;
    std::string _shownText;
    int _shownCursor = -1;
    float _blink = 0;
};

// checkertrans: makes whatever it overlaps half transparent. Applied once when the layout is
// built, as ClassicUO does (ApplyTrans), so it has no drawing of its own.
class CheckerTrans : public Control
{
public:
    CheckerTrans(float width, float height);
};

// Hue for HTML text as ClassicUO's HtmlControl.InternalBuild picks it: an explicit wire colour
// (RGB555) wins; otherwise near-black on backgrounds and white on bare gumps. RGBA8, R first.
uint32_t htmlTextColor(uint32_t wireColor, bool hasBackground, bool hasScrollbar);

}  // namespace uo::client::gumps
