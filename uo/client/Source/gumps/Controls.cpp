// SPDX-License-Identifier: BSD-2-Clause
#include "gumps/Controls.h"

#include "uo/assets/Color.h"

#include <algorithm>
#include <unordered_set>

namespace uo::client::gumps
{

namespace
{

ax::Sprite* topLeftSprite(ax::Texture2D* tex)
{
    if (!tex)
    {
        return nullptr;
    }

    auto* s = ax::Sprite::createWithTexture(tex);
    s->setAnchorPoint(ax::Vec2(0, 1));
    return s;
}

ax::Size textureSize(ax::Texture2D* tex)
{
    return tex ? tex->getContentSize() : ax::Size(0, 0);
}

// Adds a sprite to a control at UO local (x, y).
void addSpriteAt(Control* parent, ax::Sprite* s, float x, float y)
{
    if (!s)
    {
        return;
    }

    s->setPosition(ax::Vec2(x, parent->getContentSize().height - y));
    parent->addChild(s);
}

void addNodeAt(Control* parent, ax::Node* n, float x, float y)
{
    if (!n)
    {
        return;
    }

    n->setIgnoreAnchorPointForPosition(false);
    n->setAnchorPoint(ax::Vec2(0, 1));
    n->setPosition(ax::Vec2(x, parent->getContentSize().height - y));
    parent->addChild(n);
}

// Stand-in for missing art: a neutral box, so the layout and the hit area stay as designed.
void addPlaceholder(Control* parent, float w, float h, bool highlighted = false)
{
    if (w <= 0 || h <= 0)
    {
        return;
    }

    auto* box = ax::LayerColor::create(highlighted ? ax::Color32(150, 140, 110, 255) : ax::Color32(90, 85, 75, 255),
                                       w, h);
    addNodeAt(parent, box, 0, 0);
}

bool hasResizeSet(GumpTextures& textures, uint16_t graphic)
{
    for (int i = 0; i < 9; ++i)
    {
        if (!textures.gump(static_cast<uint16_t>(graphic + i)))
        {
            return false;
        }
    }

    return true;
}

// Resize backgrounds that ModernUO scripts use but older gumpart.mul files (T2A-era, UOR)
// lack: a dark frame maps to the black-and-gold 2620 set and anything else to the 2600
// scroll set, the backgrounds every client since T2A ships.
constexpr uint16_t kLightResize = 2600;
constexpr uint16_t kDarkResize = 2620;

bool isDarkResize(uint16_t graphic)
{
    switch (graphic)
    {
        case 2620: case 3600: case 5054: case 5120: case 9250: case 9260: case 9270: case 9300: case 9380: case 9390:
            return true;
        default:
            return false;
    }
}

// The resize set to draw for `graphic`: itself when the files have all nine pieces, else the
// closest T2A-era set that is complete, else 0 (draw a placeholder).
uint16_t resolveResizeGraphic(GumpTextures& textures, uint16_t graphic)
{
    if (hasResizeSet(textures, graphic))
    {
        return graphic;
    }

    const uint16_t first = isDarkResize(graphic) ? kDarkResize : kLightResize;
    const uint16_t second = first == kDarkResize ? kLightResize : kDarkResize;

    for (uint16_t candidate : {first, second})
    {
        if (candidate != graphic && hasResizeSet(textures, candidate))
        {
            reportMissingGump(graphic, candidate == kDarkResize ? "resizepic drawn with 2620" : "resizepic drawn with 2600");
            return candidate;
        }
    }

    reportMissingGump(graphic, "resizepic drawn as a placeholder");
    return 0;
}

constexpr float kPlaceholderSize = 16;

constexpr uint16_t kHtmlBackground = 0x2486;
constexpr uint16_t kScrollBackground = 257;
constexpr uint16_t kScrollSlider = 254;
constexpr uint16_t kScrollFlag = 0x0828;

}  // namespace

// Logs a gump art id the client files lack, once per id per run.
void reportMissingGump(uint16_t id, const char* use)
{
    static std::unordered_set<uint16_t> reported;

    if (reported.insert(id).second)
    {
        AXLOGW("gumps: gump art {} (0x{:04X}) missing from gumpart; {}", id, id, use);
    }
}

// --- TiledTexture ----------------------------------------------------------------------

TiledTexture* TiledTexture::create(ax::Texture2D* texture, float width, float height)
{
    auto* node = new TiledTexture();
    node->init();
    node->autorelease();
    node->setAnchorPoint(ax::Vec2(0, 1));
    node->setContentSize(ax::Size(std::max(0.0f, width), std::max(0.0f, height)));

    if (!texture || width <= 0 || height <= 0)
    {
        return node;
    }

    const ax::Size ts = texture->getContentSize();

    if (ts.width <= 0 || ts.height <= 0)
    {
        return node;
    }

    for (float y = 0; y < height; y += ts.height)
    {
        for (float x = 0; x < width; x += ts.width)
        {
            float w = std::min(ts.width, width - x);
            float h = std::min(ts.height, height - y);
            auto* s = ax::Sprite::createWithTexture(texture, ax::Rect(0, 0, w, h));
            s->setAnchorPoint(ax::Vec2(0, 1));
            s->setPosition(ax::Vec2(x, height - y));
            node->addChild(s);
        }
    }

    return node;
}

void TiledTexture::setColorAll(const ax::Color32& c)
{
    for (auto* child : getChildren())
    {
        child->setColor(c);
    }
}

void TiledTexture::setOpacityAll(uint8_t alpha)
{
    for (auto* child : getChildren())
    {
        child->setOpacity(alpha);
    }
}

// --- GumpPic ---------------------------------------------------------------------------

GumpPic::GumpPic(GumpContext& ctx, uint16_t graphic, uint16_t hue, bool partialHue) : _ctx(ctx)
{
    init();
    autorelease();
    setAcceptsInput(true);
    setGraphic(graphic, hue, partialHue);
}

void GumpPic::setGraphic(uint16_t graphic, uint16_t hue, bool partialHue)
{
    _graphic = graphic;

    if (_sprite)
    {
        _sprite->removeFromParent();
        _sprite = nullptr;
    }

    auto* tex = _ctx.textures->gump(graphic, hue, partialHue);

    if (!tex && graphic != 0)
    {
        reportMissingGump(graphic, "gumppic skipped");
    }

    setUOSize(textureSize(tex).width, textureSize(tex).height);
    _sprite = topLeftSprite(tex);
    addSpriteAt(this, _sprite, 0, 0);
}

bool GumpPic::containsLocal(const ax::Vec2& local) const
{
    if (!Control::containsLocal(local))
    {
        return false;
    }

    return _byBounds || _ctx.textures->gumpOpaqueAt(_graphic, static_cast<int>(local.x), static_cast<int>(local.y));
}

// --- GumpPicTiled ----------------------------------------------------------------------

GumpPicTiled::GumpPicTiled(GumpContext& ctx, uint16_t graphic, float width, float height, uint16_t hue)
{
    init();
    autorelease();
    setAcceptsInput(true);
    setUOSize(width, height);
    auto* texture = ctx.textures->gump(graphic, hue);

    if (!texture)
    {
        // Tiled pictures are decoration over a background; the layout does not need them.
        reportMissingGump(graphic, "gumppictiled skipped");
        return;
    }

    addNodeAt(this, TiledTexture::create(texture, width, height), 0, 0);
}

// --- ResizePic -------------------------------------------------------------------------

ResizePic::ResizePic(GumpContext& ctx, uint16_t graphic, float width, float height) : _ctx(ctx), _graphic(graphic)
{
    init();
    autorelease();
    setAcceptsInput(true);
    setUOSize(width, height);
    build();
}

void ResizePic::resize(float width, float height)
{
    setUOSize(width, height);
    build();
}

void ResizePic::build()
{
    removeAllChildren();

    // Logical piece i -> graphic offset, as ClassicUO's ResizePic.GetTexture: corners and
    // edges in reading order, with the centre (graphic + 4) last.
    static constexpr int kOffset[9] = {0, 1, 2, 3, 5, 6, 7, 8, 4};
    ax::Texture2D* t[9];
    ax::Size b[9];

    const float W = getContentSize().width;
    const float H = getContentSize().height;
    const uint16_t graphic = resolveResizeGraphic(*_ctx.textures, _graphic);

    if (graphic == 0)
    {
        addPlaceholder(this, W, H);
        return;
    }

    for (int i = 0; i < 9; ++i)
    {
        t[i] = _ctx.textures->gump(static_cast<uint16_t>(graphic + kOffset[i]));
        b[i] = textureSize(t[i]);
    }

    const float offsetTop = std::max(b[0].height, b[2].height) - b[1].height;
    const float offsetBottom = std::max(b[5].height, b[7].height) - b[6].height;
    const float offsetLeft = std::abs(std::max(b[0].width, b[5].width) - b[2].width);
    const float offsetRight = std::max(b[2].width, b[7].width) - b[4].width;

    auto tile = [&](int i, float x, float y, float w, float h) {
        if (t[i] && w > 0 && h > 0)
        {
            addNodeAt(this, TiledTexture::create(t[i], w, h), x, y);
        }
    };
    auto draw = [&](int i, float x, float y) { addSpriteAt(this, topLeftSprite(t[i]), x, y); };

    // Centre first so the frame draws over it.
    tile(8, b[0].width, b[0].height, (W - b[0].width - b[2].width) + (offsetLeft + offsetRight),
         H - b[2].height - b[7].height);
    draw(0, 0, 0);
    tile(1, b[0].width, 0, W - b[0].width - b[2].width, b[1].height);
    draw(2, W - b[2].width, offsetTop);
    tile(3, 0, b[0].height, b[3].width, H - b[0].height - b[5].height);
    tile(4, W - b[4].width, b[2].height, b[4].width, H - b[2].height - b[7].height);
    draw(5, 0, H - b[5].height);
    tile(6, b[5].width, H - b[6].height - offsetBottom, W - b[5].width - b[7].width, b[6].height);
    draw(7, W - b[7].width, H - b[7].height);
}

// --- GumpButton ------------------------------------------------------------------------

GumpButton::GumpButton(GumpContext& ctx, uint16_t normal, uint16_t pressed, uint16_t over)
    : _ctx(ctx), _normal(normal), _pressed(pressed), _over(over)
{
    init();
    autorelease();
    setAcceptsInput(true);
    setMovesGump(false);
    setGraphics(normal, pressed, over);
}

void GumpButton::setGraphics(uint16_t normal, uint16_t pressed, uint16_t over)
{
    _normal = normal;
    _pressed = pressed;
    _over = over;
    auto* texture = _ctx.textures->gump(normal);

    if (!texture)
    {
        reportMissingGump(normal, "button drawn as a placeholder");
    }

    auto size = texture ? textureSize(texture) : ax::Size(kPlaceholderSize, kPlaceholderSize);
    setUOSize(size.width, size.height);
    showState();
}

void GumpButton::showState()
{
    uint16_t g = _normal;

    if (_isPressed && _isHovered)
    {
        g = _pressed;
    }
    else if (_isHovered && _over != 0)
    {
        g = _over;
    }

    if (_sprite)
    {
        _sprite->removeFromParent();
    }

    if (_placeholder)
    {
        _placeholder->removeFromParent();
        _placeholder = nullptr;
    }

    _sprite = topLeftSprite(_ctx.textures->gump(g));

    if (!_sprite && !_ctx.textures->gump(_normal))
    {
        addPlaceholder(this, getContentSize().width, getContentSize().height, _isPressed || _isHovered);
        _placeholder = getChildren().back();
        return;
    }

    addSpriteAt(this, _sprite, 0, 0);
}

void GumpButton::onMouseDown(MouseButton button, const ax::Vec2&)
{
    if (button == MouseButton::Left)
    {
        _isPressed = true;
        _isHovered = true;
        showState();
    }
}

void GumpButton::onMouseUp(MouseButton, bool)
{
    _isPressed = false;
    showState();
}

void GumpButton::onHover(bool entered)
{
    _isHovered = entered;
    showState();
}

void GumpButton::onClick(MouseButton button)
{
    if (button != MouseButton::Left)
    {
        return;
    }

    if (onClicked)
    {
        onClicked(this);
        return;
    }

    if (auto* g = gump())
    {
        if (_activates)
        {
            g->onButton(_buttonId);
        }
        else
        {
            g->setActivePage(_toPage);
        }
    }
}

bool GumpButton::containsLocal(const ax::Vec2& local) const
{
    // Server buttons test their bounds (ClassicUO sets ContainsByBounds on them).
    return Control::containsLocal(local);
}

// --- ButtonTileArt ---------------------------------------------------------------------

ButtonTileArt::ButtonTileArt(GumpContext& ctx, uint16_t normal, uint16_t pressed, uint16_t tile, uint16_t tileHue,
                             float cellWidth, float cellHeight)
    : GumpButton(ctx, normal, pressed)
{
    if (cellWidth > 0 && cellHeight > 0)
    {
        setUOSize(cellWidth, cellHeight);
    }

    auto* tex = ctx.textures->art(tile, tileHue, ctx.textures->artIsPartialHue(tile));

    if (auto* s = topLeftSprite(tex))
    {
        auto ts = textureSize(tex);
        // Centred in the cell, never offset by its size (see ButtonTileArt.cs).
        float x = std::max(0.0f, (cellWidth - ts.width) / 2);
        float y = std::max(0.0f, (cellHeight - ts.height) / 2);
        addSpriteAt(this, s, x, y);
    }
}

bool ButtonTileArt::containsLocal(const ax::Vec2& local) const
{
    return Control::containsLocal(local);
}

// --- Checkbox --------------------------------------------------------------------------

Checkbox::Checkbox(GumpContext& ctx, uint16_t unchecked, uint16_t checked, bool isChecked, bool isRadio, int group)
    : _ctx(ctx), _uncheckedGraphic(unchecked), _checkedGraphic(checked), _checked(isChecked), _radio(isRadio),
      _group(group)
{
    init();
    autorelease();
    setAcceptsInput(true);
    setMovesGump(false);
    auto* texture = _ctx.textures->gump(unchecked);

    if (!texture)
    {
        reportMissingGump(unchecked, "checkbox drawn as a placeholder");
    }

    auto size = texture ? textureSize(texture) : ax::Size(kPlaceholderSize, kPlaceholderSize);
    setUOSize(size.width, size.height);
    setChecked(isChecked);
}

void Checkbox::setChecked(bool v)
{
    _checked = v;

    if (_sprite)
    {
        _sprite->removeFromParent();
    }

    if (_placeholder)
    {
        _placeholder->removeFromParent();
        _placeholder = nullptr;
    }

    _sprite = topLeftSprite(_ctx.textures->gump(_checked ? _checkedGraphic : _uncheckedGraphic));

    if (!_sprite)
    {
        addPlaceholder(this, getContentSize().width, getContentSize().height, _checked);
        _placeholder = getChildren().back();
        return;
    }

    addSpriteAt(this, _sprite, 0, 0);
}

void Checkbox::onClick(MouseButton button)
{
    if (button != MouseButton::Left)
    {
        return;
    }

    if (_radio)
    {
        if (_checked)
        {
            return;
        }

        // Uncheck the rest of the group within the same parent, like RadioButton.cs.
        if (auto* parent = getParent())
        {
            for (auto* n : parent->getChildren())
            {
                if (auto* other = dynamic_cast<Checkbox*>(n); other && other != this && other->_radio &&
                                                              other->_group == _group && other->_checked)
                {
                    other->setChecked(false);
                }
            }
        }

        setChecked(true);
    }
    else
    {
        setChecked(!_checked);
    }

    if (onChanged)
    {
        onChanged(this);
    }
}

// --- StaticPic -------------------------------------------------------------------------

StaticPic::StaticPic(GumpContext& ctx, uint16_t graphic, uint16_t hue) : _ctx(ctx)
{
    init();
    autorelease();
    setAcceptsInput(true);
    setGraphic(graphic, hue);
}

void StaticPic::setGraphic(uint16_t graphic, uint16_t hue)
{
    _graphic = graphic;

    if (_sprite)
    {
        _sprite->removeFromParent();
    }

    auto* tex = _ctx.textures->art(graphic, hue, hue != 0 && _ctx.textures->artIsPartialHue(graphic));
    auto size = textureSize(tex);
    setUOSize(size.width, size.height);
    _sprite = topLeftSprite(tex);
    addSpriteAt(this, _sprite, 0, 0);
}

bool StaticPic::containsLocal(const ax::Vec2& local) const
{
    return Control::containsLocal(local) &&
           _ctx.textures->artOpaqueAt(_graphic, static_cast<int>(local.x), static_cast<int>(local.y));
}

// --- GumpPicInPic ----------------------------------------------------------------------

GumpPicInPic::GumpPicInPic(GumpContext& ctx, uint16_t graphic, float sx, float sy, float width, float height,
                           uint16_t hue, bool partialHue)
{
    init();
    autorelease();
    setAcceptsInput(true);
    setUOSize(width, height);

    if (auto* tex = ctx.textures->gump(graphic, hue, partialHue))
    {
        auto ts = textureSize(tex);
        float w = std::min(width, ts.width - sx);
        float h = std::min(height, ts.height - sy);

        if (w > 0 && h > 0)
        {
            auto* s = ax::Sprite::createWithTexture(tex, ax::Rect(sx, sy, w, h));
            s->setAnchorPoint(ax::Vec2(0, 1));
            addSpriteAt(this, s, 0, 0);
        }
    }
}

// --- TextLabel -------------------------------------------------------------------------

TextLabel::TextLabel(GumpContext& ctx, std::string_view text, const GumpTextStyle& style, float cropWidth,
                     float cropHeight)
    : _ctx(ctx), _style(style), _cropW(cropWidth), _cropH(cropHeight)
{
    init();
    autorelease();
    setText(text);
}

void TextLabel::setText(std::string_view text)
{
    _text = std::string(text);

    if (_label)
    {
        _label->removeFromParent();
        _label = nullptr;
    }

    _label = _ctx.text->createLabel(_text, _style);
    ax::Size size = _label ? _label->getContentSize() : ax::Size(0, 0);

    if (_cropW > 0 || _cropH > 0)
    {
        size = ax::Size(_cropW > 0 ? _cropW : size.width, _cropH > 0 ? _cropH : size.height);
    }

    setUOSize(size.width, size.height);
    addNodeAt(this, _label, 0, 0);
}

// --- HtmlArea --------------------------------------------------------------------------

uint32_t htmlTextColor(uint32_t wireColor, bool hasBackground, bool hasScrollbar)
{
    auto rgba = [](uint8_t r, uint8_t g, uint8_t b) { return (uint32_t{r} << 24) | (uint32_t{g} << 16) | (uint32_t{b} << 8) | 0xFF; };

    if (wireColor > 0)
    {
        if (wireColor == 0x00FFFFFF || wireColor == 0xFFFF || wireColor == 0xFF)
        {
            return rgba(0xFF, 0xFF, 0xFE);
        }

        uint16_t c = static_cast<uint16_t>(wireColor);
        return rgba(uo::assets::kColor5To8[(c >> 10) & 0x1F], uo::assets::kColor5To8[(c >> 5) & 0x1F],
                    uo::assets::kColor5To8[c & 0x1F]);
    }

    // No colour: near-black text, except on a bare gump with a scrollbar, where it is white.
    return !hasBackground && hasScrollbar ? rgba(0xFF, 0xFF, 0xFF) : rgba(0x01, 0x01, 0x01);
}

HtmlArea::HtmlArea(GumpContext& ctx, std::string_view html, float width, float height, bool background,
                   int scrollStyle, uint32_t color)
    : _ctx(ctx), _hasScrollbar(scrollStyle != 0)
{
    init();
    autorelease();
    setAcceptsInput(true);
    setUOSize(width, height);
    _viewH = height;

    const float barW = _hasScrollbar ? 16 : 0;
    const float pad = background ? 4 : 0;

    if (background)
    {
        auto* bg = new ResizePic(ctx, kHtmlBackground, width - barW, height);
        bg->setAcceptsInput(false);
        addChild(bg);
        bg->setUOPosition(0, 0);
    }

    // MaxWidth as HtmlControl: minus the scrollbar and the background padding.
    const int textWidth = static_cast<int>(width - barW - (background ? 8 : 0) - (background ? 9 : 0));
    _content = ctx.text->createHtml(html, std::max(1, textWidth), htmlTextColor(color, background, _hasScrollbar),
                                    background);

    _clip = ax::ClippingRectangleNode::create(ax::Rect(pad, pad, width - barW - 2 * pad, height - 2 * pad));
    addChild(_clip);

    if (_content)
    {
        _content->setIgnoreAnchorPointForPosition(false);
        _content->setAnchorPoint(ax::Vec2(0, 1));
        _clip->addChild(_content);
        _maxScroll = std::max(0.0f, _content->getContentSize().height - height + (background ? 8 : 0));
    }

    if (_hasScrollbar)
    {
        if (scrollStyle == 1)
        {
            addNodeAt(this, TiledTexture::create(ctx.textures->gump(kScrollBackground), 15, height), width - 14, 0);
        }

        _thumb = topLeftSprite(ctx.textures->gump(scrollStyle == 2 ? kScrollFlag : kScrollSlider));

        if (_thumb)
        {
            addChild(_thumb);
        }
    }

    updateScroll();
}

void HtmlArea::updateScroll()
{
    if (_content)
    {
        float padding = _clip->getClippingRegion().origin.x;
        _content->setPosition(ax::Vec2(padding, getContentSize().height - padding + _scroll));
    }

    if (_thumb)
    {
        float range = std::max(0.0f, _viewH - _thumb->getContentSize().height);
        float t = _maxScroll > 0 ? _scroll / _maxScroll : 0;
        _thumb->setPosition(ax::Vec2(getContentSize().width - 14, getContentSize().height - t * range));
    }
}

void HtmlArea::scrollBy(float dy)
{
    _scroll = std::clamp(_scroll + dy, 0.0f, _maxScroll);
    updateScroll();
}

bool HtmlArea::containsLocal(const ax::Vec2& local) const
{
    return Control::containsLocal(local);
}

void HtmlArea::onMouseDown(MouseButton button, const ax::Vec2& local)
{
    // Clicking the scrollbar track pages up or down.
    if (_hasScrollbar && button == MouseButton::Left && local.x >= getContentSize().width - 16)
    {
        float thumbY = _maxScroll > 0 ? (_scroll / _maxScroll) * _viewH : 0;
        scrollBy(local.y < thumbY ? -_viewH : _viewH);
    }
}

// --- TextEntry -------------------------------------------------------------------------

TextEntry::TextEntry(GumpContext&, float width, float height, uint16_t hue, std::string_view text, int maxLength)
{
    init();
    autorelease();
    setAcceptsInput(true);
    setMovesGump(false);
    setUOSize(width, height);

    // createGumpEntry adds 1 to the wire hue itself.
    _box = uo::client::text::TextBox::createGumpEntry(width, height, static_cast<uint16_t>(hue - 1), text, maxLength);

    if (_box)
    {
        _box->setPosition(ax::Vec2::zero);
        addChild(_box);
    }
}

std::string TextEntry::text() const
{
    return _box ? _box->text() : std::string{};
}

void TextEntry::focus()
{
    if (_box)
    {
        _box->focus();
    }
}

void TextEntry::onMouseDown(MouseButton button, const ax::Vec2& local)
{
    if (button == MouseButton::Left && _box)
    {
        _box->focus();
        _box->placeCaretAt(static_cast<int>(local.x), static_cast<int>(local.y));
    }
}

// --- CheckerTrans ----------------------------------------------------------------------

CheckerTrans::CheckerTrans(float width, float height)
{
    init();
    autorelease();
    setUOSize(width, height);
}

}  // namespace uo::client::gumps
