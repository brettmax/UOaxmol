// SPDX-License-Identifier: BSD-2-Clause
// Ported from ClassicUO (StbTextBox.cs). An editable text entry drawn with the UO fonts, in
// place of ax::ui::InputField and its TTF text.

#pragma once

#include "axmol/TextFactory.h"

#include "uo/text/TextEdit.h"

#include "axmol/scene/Node.h"
#include "axmol/base/InputDelegate.h"

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <utility>

namespace ax
{
class ClippingRectangleNode;
class KeyboardEventListener;
}  // namespace ax

namespace uo::client::text
{

struct TextBoxStyle
{
    std::uint8_t font = 1;  // 0xFF = client default
    bool unicode      = true;
    std::uint16_t hue = 0;
    // Black outline, as server gump entries use. Other uo::text::FontStyle bits go in extraFlags.
    bool border               = true;
    std::uint16_t extraFlags  = 0;
    uo::text::TextAlign align = uo::text::TextAlign::Left;
    int maxChars              = -1;  // -1 = no limit
    bool multiline            = false;
    bool numbersOnly          = false;
    bool password             = false;  // draws '*' for every character
};

// A width x height box of editable UO-font text, clipped to its bounds like StbTextBox.
// The text wraps at `width` and the caret is the font's "_" drawn where GetCaretPos* puts it
// while the box has keyboard focus. Local coordinates run from the top-left corner, y down;
// the node's content size is (width, height) with its origin at the bottom-left, as any node.
//
// Keys: typing, Backspace, Delete, Left/Right, Home/End (line start and end), Up/Down in a
// multiline box, Ctrl+V to paste and Ctrl+C to copy (not in password boxes). Enter calls
// onSubmit in a single-line box and adds a line in a multiline one; Escape blurs.
class TextBox : public ax::Node, public ax::InputDelegate
{
public:
    static TextBox* create(float width, float height, const TextBoxStyle& style = {});

    // A server gump textentry / textentrylimited as ClassicUO builds it: unicode font 1,
    // black border, hue + 1, 255 characters unless limited.
    static TextBox* createGumpEntry(float width, float height, std::uint16_t hue, std::string_view text,
                                    int maxChars = 0);

    // UTF-8.
    std::string text() const;
    void setText(std::string_view utf8);
    const std::u16string& text16() const { return _edit.text(); }

    int caretIndex() const { return _edit.caret(); }
    void setCaretIndex(int index);

    const TextBoxStyle& style() const { return _style; }
    void setHue(std::uint16_t hue);

    // Keyboard focus: attaches to the IME (and the on-screen keyboard where there is one).
    void focus();
    void blur();
    bool focused() const { return _focused; }

    // Moves the caret to the character nearest a point: local top-left coordinates, or a
    // world-space point (as a pointer event gives), which returns false when outside the box.
    void placeCaretAt(int x, int y);
    bool placeCaretAtWorld(const ax::Vec2& world);

    std::function<void(TextBox*)> onChanged;
    std::function<void(TextBox*)> onSubmit;
    std::function<void(TextBox*, bool focused)> onFocusChanged;

    bool init(float width, float height, const TextBoxStyle& style);

    // ax::InputDelegate
    bool attachWithIME() override;
    bool detachWithIME() override;
    void performEditAction(ax::EditAction action) override;

protected:
    bool canAttachWithIME() const override { return true; }
    bool canDetachWithIME() const override { return true; }
    void didDetachWithIME() override;
    void insertText(std::string_view text) override;
    void deleteBackward(unsigned int numChars) override;
    void controlKey(ax::KeyboardEvent::KeyCode keyCode) override;
    bool hitTestWithIME(const ax::Vec2& location) override;

    void onEnter() override;
    void onExit() override;

private:
    TextStyle labelStyle() const;
    std::u16string shownText() const;
    // The caret's top-left in local UO coordinates.
    std::pair<int, int> caretPoint() const;
    void rebuildText();
    void updateCaret();
    void changed();
    void moveVertically(int lines);
    void pasteClipboard();

    TextBoxStyle _style;
    uo::text::TextEdit _edit;
    int _width  = 0;
    int _height = 0;
    bool _focused = false;
    ax::ClippingRectangleNode* _clip = nullptr;
    TextLabel* _label                = nullptr;
    TextLabel* _caret                = nullptr;
    ax::KeyboardEventListener* _keys = nullptr;
};

}  // namespace uo::client::text
