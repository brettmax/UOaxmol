// SPDX-License-Identifier: BSD-2-Clause
#include "Json.h"

#include <cmath>
#include <cstdio>
#include <fstream>

namespace uoconvert
{

JsonWriter& JsonWriter::value(double d)
{
    separator();
    if (!std::isfinite(d))
    {
        _out += "null";
        return *this;
    }
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.6g", d);
    _out += buf;
    return *this;
}

JsonWriter& JsonWriter::open(char c)
{
    separator();
    _out += c;
    _first.push_back(true);
    return *this;
}

JsonWriter& JsonWriter::close(char c)
{
    bool empty = _first.back();
    _first.pop_back();
    if (!empty)
        newline();
    _out += c;
    return *this;
}

void JsonWriter::newline()
{
    if (!_pretty)
        return;
    _out += '\n';
    _out.append(_first.size() * 2, ' ');
}

void JsonWriter::separator()
{
    if (_afterKey)
    {
        _afterKey = false;
        return;
    }
    if (_first.empty())
        return;
    if (!_first.back())
        _out += ',';
    _first.back() = false;
    newline();
}

void JsonWriter::string(std::string_view s)
{
    _out += '"';
    for (unsigned char c : s)
    {
        switch (c)
        {
        case '"': _out += "\\\""; break;
        case '\\': _out += "\\\\"; break;
        case '\n': _out += "\\n"; break;
        case '\r': _out += "\\r"; break;
        case '\t': _out += "\\t"; break;
        default:
            if (c < 0x20)
            {
                char buf[8];
                std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                _out += buf;
            }
            else
                _out += static_cast<char>(c);
        }
    }
    _out += '"';
}

bool JsonWriter::save(const std::string& path) const
{
    std::ofstream f(path, std::ios::binary);
    f << _out;
    if (_pretty)
        f << '\n';
    return static_cast<bool>(f);
}

}  // namespace uoconvert

namespace uoconvert
{
std::string latin1ToUtf8(std::string_view s)
{
    std::string out;
    out.reserve(s.size());
    for (unsigned char c : s)
    {
        if (c == 0)
            break;
        if (c < 0x80)
            out += static_cast<char>(c);
        else
        {
            out += static_cast<char>(0xC0 | (c >> 6));
            out += static_cast<char>(0x80 | (c & 0x3F));
        }
    }
    return out;
}
}  // namespace uoconvert
