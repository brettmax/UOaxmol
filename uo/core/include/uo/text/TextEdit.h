// SPDX-License-Identifier: BSD-2-Clause
// Ported from ClassicUO (StbTextBox.cs): the text-entry editing rules, without the drawing.

#pragma once

#include <functional>
#include <string>
#include <string_view>

namespace uo::text
{

// The editable text and caret of a text entry, as StbTextBox keeps them: a character limit
// (maxChars, -1 for none) that truncates set text and clips input to the room left, an
// optional digits-only filter, and typed characters accepted only when the font can draw
// them. Selection is not kept; the caret is an index from 0 to text().size().
class TextEdit
{
public:
    // Whether a single typed character can be drawn (GetCharWidth > 0 in the original).
    using Printable = std::function<bool(char16_t)>;

    explicit TextEdit(int maxChars = -1, bool multiline = false) : _maxChars(maxChars), _multiline(multiline) {}

    const std::u16string& text() const { return _text; }
    int caret() const { return _caret; }
    int maxChars() const { return _maxChars; }
    bool multiline() const { return _multiline; }

    void setMaxChars(int maxChars);
    void setMultiline(bool value) { _multiline = value; }
    void setNumbersOnly(bool value) { _numbersOnly = value; }
    void setPrintable(Printable printable) { _printable = std::move(printable); }

    // Replaces the text (cut to maxChars) and puts the caret at its end. Returns whether the
    // text changed.
    bool setText(std::u16string_view text);
    void setCaret(int index);

    // Typed or pasted input at the caret. One character must be printable (or a newline in
    // a multiline entry); longer input is pasted as is, cut to the room left. Carriage
    // returns are dropped and, in a single-line entry, newlines too. Returns whether the
    // text changed.
    bool insert(std::u16string_view input);
    bool deleteBackward(int count = 1);
    bool deleteForward();

    void moveLeft() { setCaret(_caret - 1); }
    void moveRight() { setCaret(_caret + 1); }
    void moveHome() { _caret = 0; }
    void moveEnd() { _caret = static_cast<int>(_text.size()); }

private:
    std::u16string _text;
    int _caret        = 0;
    int _maxChars     = -1;
    bool _multiline   = false;
    bool _numbersOnly = false;
    Printable _printable;
};

}  // namespace uo::text
