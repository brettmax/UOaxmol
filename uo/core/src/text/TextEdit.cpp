// SPDX-License-Identifier: BSD-2-Clause
// Ported from ClassicUO (StbTextBox.cs: Text, SetText, OnTextInput).

#include "uo/text/TextEdit.h"

#include <algorithm>

namespace uo::text
{

void TextEdit::setMaxChars(int maxChars)
{
    _maxChars = maxChars;

    if (_maxChars > 0 && static_cast<int>(_text.size()) > _maxChars)
    {
        _text.resize(static_cast<size_t>(_maxChars));
        _caret = std::min(_caret, _maxChars);
    }
}

bool TextEdit::setText(std::u16string_view text)
{
    if (_maxChars > 0 && static_cast<int>(text.size()) > _maxChars)
        text = text.substr(0, static_cast<size_t>(_maxChars));

    const bool changed = _text != text;
    _text              = std::u16string(text);
    _caret             = static_cast<int>(_text.size());
    return changed;
}

void TextEdit::setCaret(int index)
{
    _caret = std::clamp(index, 0, static_cast<int>(_text.size()));
}

bool TextEdit::insert(std::u16string_view input)
{
    std::u16string c;
    c.reserve(input.size());

    for (char16_t ch : input)
    {
        if (ch == u'\r' || (ch == u'\n' && !_multiline))
            continue;

        c.push_back(ch);
    }

    if (c.empty())
        return false;

    if (_maxChars > 0)
    {
        const int remains = _maxChars - static_cast<int>(_text.size());

        if (remains <= 0)
            return false;

        if (static_cast<int>(c.size()) > remains)
            c.resize(static_cast<size_t>(remains));
    }

    if (_numbersOnly && !std::all_of(c.begin(), c.end(), [](char16_t ch) { return ch >= u'0' && ch <= u'9'; }))
        return false;

    if (c.size() == 1 && c[0] != u'\n' && _printable && !_printable(c[0]))
        return false;

    _text.insert(static_cast<size_t>(_caret), c);
    _caret += static_cast<int>(c.size());
    return true;
}

bool TextEdit::deleteBackward(int count)
{
    count = std::min(count, _caret);

    if (count <= 0)
        return false;

    _text.erase(static_cast<size_t>(_caret - count), static_cast<size_t>(count));
    _caret -= count;
    return true;
}

bool TextEdit::deleteForward()
{
    if (_caret >= static_cast<int>(_text.size()))
        return false;

    _text.erase(static_cast<size_t>(_caret), 1);
    return true;
}

}  // namespace uo::text
