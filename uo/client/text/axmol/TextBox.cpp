// SPDX-License-Identifier: BSD-2-Clause
// Ported from ClassicUO (StbTextBox.cs).

#include "axmol/TextBox.h"

#include "axmol/TextSystem.h"

#include "uo/text/Utf.h"

#include "axmol/2d/ClippingRectangleNode.h"
#include "axmol/base/Director.h"
#include "axmol/base/EventDispatcher.h"
#include "axmol/base/KeyboardEventListener.h"
#include "axmol/platform/Device.h"
#include "axmol/platform/RenderView.h"
#include "axmol/base/RefPtr.h"

#include <algorithm>
#include <climits>

namespace uo::client::text
{

TextBox* TextBox::create(float width, float height, const TextBoxStyle& style)
{
    auto* box = new TextBox();

    if (box->init(width, height, style))
    {
        box->autorelease();
        return box;
    }

    delete box;
    return nullptr;
}

TextBox* TextBox::createGumpEntry(float width, float height, std::uint16_t hue, std::string_view text, int maxChars)
{
    TextBoxStyle style;
    style.font     = 1;
    style.unicode  = true;
    style.border   = true;
    style.hue      = static_cast<std::uint16_t>(hue + 1);
    style.maxChars = maxChars > 0 ? maxChars : 255;

    TextBox* box = create(width, height, style);

    if (box)
        box->setText(text);

    return box;
}

bool TextBox::init(float width, float height, const TextBoxStyle& style)
{
    if (!ax::Node::init())
        return false;

    _style  = style;
    _width  = static_cast<int>(width);
    _height = static_cast<int>(height);
    setContentSize(ax::Size(width, height));

    _edit.setMaxChars(style.maxChars);
    _edit.setMultiline(style.multiline);
    _edit.setNumbersOnly(style.numbersOnly);
    _edit.setPrintable([this](char16_t c) {
        auto& system = TextSystem::instance();

        if (!system.ready())
            return c >= u' ';

        const std::uint8_t font = system.resolveFont(_style.font);
        const char16_t shown    = _style.password ? u'*' : c;
        return (_style.unicode ? system.fonts().charWidthUnicode(font, shown)
                               : system.fonts().charWidthAscii(font, shown)) > 0;
    });

    _clip = ax::ClippingRectangleNode::create(ax::Rect(0, 0, width, height));
    addChild(_clip);

    _label = TextLabel::create("", labelStyle());
    _label->setPosition(ax::Vec2(0, height));
    _clip->addChild(_label);

    // StbTextBox's caret is the font's "_" with the text's border.
    _caret = TextLabel::create("_", labelStyle());
    _caret->setVisible(false);
    _clip->addChild(_caret);

    updateCaret();
    return true;
}

TextStyle TextBox::labelStyle() const
{
    TextStyle s;
    s.font       = _style.font;
    s.unicode    = _style.unicode;
    s.hue        = _style.hue;
    s.border     = _style.border;
    s.extraFlags = _style.extraFlags;
    s.align      = _style.align;
    s.maxWidth   = _width;
    return s;
}

std::u16string TextBox::shownText() const
{
    return _style.password ? std::u16string(_edit.text().size(), u'*') : _edit.text();
}

std::string TextBox::text() const
{
    return uo::text::utf16ToUtf8(_edit.text());
}

void TextBox::setText(std::string_view utf8)
{
    if (_edit.setText(fontText(utf8, true)))
        changed();
    else
        updateCaret();
}

void TextBox::setCaretIndex(int index)
{
    _edit.setCaret(index);
    updateCaret();
}

void TextBox::setHue(std::uint16_t hue)
{
    if (_style.hue == hue)
        return;

    _style.hue = hue;
    _label->setHue(hue);
    _caret->setHue(hue);
    updateCaret();  // a rebuilt label shows itself
}

void TextBox::rebuildText()
{
    _label->setText(uo::text::utf16ToUtf8(shownText()));
}

std::pair<int, int> TextBox::caretPoint() const
{
    auto& system = TextSystem::instance();

    if (!system.ready())
        return {0, 0};

    const auto& fonts       = system.fonts();
    const std::uint8_t font = system.resolveFont(_style.font);
    const std::u16string s  = shownText();
    const auto flags        = labelStyle().flags();
    return _style.unicode ? fonts.caretPosUnicode(font, s, _edit.caret(), _width, _style.align, flags)
                          : fonts.caretPosAscii(font, s, _edit.caret(), _width, _style.align, flags);
}

void TextBox::updateCaret()
{
    const auto [x, y] = caretPoint();
    _caret->setPosition(ax::Vec2(static_cast<float>(x), static_cast<float>(_height - y)));
    _caret->setVisible(_focused && TextSystem::instance().ready());
}

void TextBox::changed()
{
    rebuildText();
    updateCaret();

    if (onChanged)
        onChanged(this);
}

void TextBox::placeCaretAt(int x, int y)
{
    auto& system = TextSystem::instance();

    if (!system.ready())
        return;

    const auto& fonts       = system.fonts();
    const std::uint8_t font = system.resolveFont(_style.font);
    const std::u16string s  = shownText();
    const auto flags        = labelStyle().flags();
    _edit.setCaret(_style.unicode ? fonts.caretIndexUnicode(font, s, x, y, _width, _style.align, flags)
                                  : fonts.caretIndexAscii(font, s, x, y, _width, _style.align, flags));
    updateCaret();
}

bool TextBox::placeCaretAtWorld(const ax::Vec2& world)
{
    const ax::Vec2 local = convertToNodeSpace(world);

    if (local.x < 0 || local.y < 0 || local.x >= _width || local.y >= _height)
        return false;

    placeCaretAt(static_cast<int>(local.x), static_cast<int>(_height - local.y));
    return true;
}

void TextBox::moveVertically(int lines)
{
    // Unicode lines are at most a glyph tall; stepping by the caret label's height less its
    // bottom margin lands inside the next line.
    const auto [x, y] = caretPoint();
    const int step    = std::max(1, static_cast<int>(_caret->getContentSize().height) - 4);
    placeCaretAt(x, y + lines * step);
}

// --- focus ---------------------------------------------------------------------------------

void TextBox::focus()
{
    attachWithIME();
}

void TextBox::blur()
{
    detachWithIME();
}

bool TextBox::attachWithIME()
{
    if (_focused)
        return true;

    if (!ax::InputDelegate::attachWithIME())
        return false;

    _focused = true;

    if (auto* view = ax::Director::getInstance()->getRenderView())
        view->setIMEKeyboardState(true);

    updateCaret();

    if (onFocusChanged)
        onFocusChanged(this, true);

    return true;
}

bool TextBox::detachWithIME()
{
    const bool ret = ax::InputDelegate::detachWithIME();

    if (ret)
    {
        if (auto* view = ax::Director::getInstance()->getRenderView())
            view->setIMEKeyboardState(false);
    }

    didDetachWithIME();
    return ret;
}

void TextBox::didDetachWithIME()
{
    // Also reached when another box takes the IME.
    if (!_focused)
        return;

    _focused = false;
    updateCaret();

    if (onFocusChanged)
        onFocusChanged(this, false);
}

bool TextBox::hitTestWithIME(const ax::Vec2& location)
{
    const ax::Vec2 local = convertToNodeSpace(location);
    return local.x >= 0 && local.y >= 0 && local.x < _width && local.y < _height;
}

// --- editing -------------------------------------------------------------------------------

void TextBox::insertText(std::string_view text)
{
    if (text == "\n" && !_style.multiline)
    {
        if (onSubmit)
            onSubmit(this);
        return;
    }

    if (_edit.insert(fontText(text, true)))
        changed();
}

void TextBox::deleteBackward(unsigned int numChars)
{
    if (_edit.deleteBackward(static_cast<int>(numChars)))
        changed();
}

void TextBox::controlKey(ax::KeyboardEvent::KeyCode keyCode)
{
    using Key = ax::KeyboardEvent::KeyCode;

    switch (keyCode)
    {
    case Key::KEY_DELETE:
    case Key::KEY_KP_DELETE:
        if (_edit.deleteForward())
            changed();
        return;
    case Key::KEY_LEFT_ARROW: _edit.moveLeft(); break;
    case Key::KEY_RIGHT_ARROW: _edit.moveRight(); break;
    case Key::KEY_HOME:
    case Key::KEY_KP_HOME:
        if (_style.multiline)
        {
            placeCaretAt(0, caretPoint().second);  // the start of the caret's line
            return;
        }
        _edit.moveHome();
        break;
    case Key::KEY_END:
        if (_style.multiline)
        {
            placeCaretAt(INT_MAX / 2, caretPoint().second);  // the end of the caret's line
            return;
        }
        _edit.moveEnd();
        break;
    case Key::KEY_ESCAPE: blur(); return;
    default: return;
    }

    updateCaret();
}

void TextBox::performEditAction(ax::EditAction action)
{
    switch (action)
    {
    case ax::EditAction::Copy:
        if (!_style.password)
            ax::Device::setClipboardText(text());
        break;
    case ax::EditAction::Cut:
        if (!_style.password)
        {
            ax::Device::setClipboardText(text());
            setText("");
        }
        break;
    case ax::EditAction::Paste: pasteClipboard(); break;
    case ax::EditAction::SelectAll: break;  // no selection
    }
}

void TextBox::pasteClipboard()
{
    ax::Device::getClipboardText([box = ax::RefPtr<TextBox>(this)](std::string_view text) {
        if (!text.empty() && box->isRunning() && box->_edit.insert(fontText(text, true)))
            box->changed();
    });
}

void TextBox::onEnter()
{
    ax::Node::onEnter();

    if (_keys)
        return;

    // The IME does not deliver shortcuts or Up/Down; take them while focused.
    _keys               = ax::KeyboardEventListener::create();
    _keys->onKeyPressed = [this](ax::KeyboardEvent* event) {
        if (!_focused)
            return;

        using Key       = ax::KeyboardEvent::KeyCode;
        const Key code  = event->getKeyCode();
        const bool ctrl = (event->getModifiers() & ax::KeyboardEvent::CONTROL) != 0;

        if (ctrl && (code == Key::KEY_V || code == Key::KEY_CAPITAL_V))
            performEditAction(ax::EditAction::Paste);
        else if (ctrl && (code == Key::KEY_C || code == Key::KEY_CAPITAL_C))
            performEditAction(ax::EditAction::Copy);
        else if (ctrl && (code == Key::KEY_X || code == Key::KEY_CAPITAL_X))
            performEditAction(ax::EditAction::Cut);
        else if (_style.multiline && code == Key::KEY_UP_ARROW)
            moveVertically(-1);
        else if (_style.multiline && code == Key::KEY_DOWN_ARROW)
            moveVertically(1);
    };
    _eventDispatcher->addEventListenerWithSceneGraphPriority(_keys, this);
}

void TextBox::onExit()
{
    if (_keys)
    {
        _eventDispatcher->removeEventListener(_keys);
        _keys = nullptr;
    }

    blur();
    ax::Node::onExit();
}

}  // namespace uo::client::text
