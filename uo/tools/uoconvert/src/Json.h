// SPDX-License-Identifier: BSD-2-Clause
// Minimal streaming JSON writer for the converter's index and manifest files.
#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace uoconvert
{

class JsonWriter
{
public:
    explicit JsonWriter(bool pretty = false) : _pretty(pretty) {}

    JsonWriter& beginObject() { return open('{'); }
    JsonWriter& endObject() { return close('}'); }
    JsonWriter& beginArray() { return open('['); }
    JsonWriter& endArray() { return close(']'); }

    JsonWriter& key(std::string_view k)
    {
        separator();
        string(k);
        _out += _pretty ? ": " : ":";
        _afterKey = true;
        return *this;
    }

    JsonWriter& value(std::string_view s)
    {
        separator();
        string(s);
        return *this;
    }
    JsonWriter& value(const char* s) { return value(std::string_view(s)); }
    JsonWriter& value(const std::string& s) { return value(std::string_view(s)); }
    JsonWriter& value(bool b)
    {
        separator();
        _out += b ? "true" : "false";
        return *this;
    }
    JsonWriter& value(double d);
    JsonWriter& value(std::int64_t v)
    {
        separator();
        _out += std::to_string(v);
        return *this;
    }
    JsonWriter& value(std::uint64_t v)
    {
        separator();
        _out += std::to_string(v);
        return *this;
    }
    JsonWriter& value(int v) { return value(static_cast<std::int64_t>(v)); }
    JsonWriter& value(unsigned v) { return value(static_cast<std::uint64_t>(v)); }
    JsonWriter& null()
    {
        separator();
        _out += "null";
        return *this;
    }

    // Splices already-serialised JSON in as one value.
    JsonWriter& raw(std::string_view json)
    {
        separator();
        _out += json;
        return *this;
    }

    template <class T>
    JsonWriter& field(std::string_view k, const T& v)
    {
        key(k);
        return value(v);
    }

    const std::string& str() const { return _out; }
    bool save(const std::string& path) const;

private:
    JsonWriter& open(char c);
    JsonWriter& close(char c);
    void separator();
    void newline();
    void string(std::string_view s);

    std::string _out;
    std::vector<bool> _first;  // per open container: nothing written yet
    bool _afterKey = false;
    bool _pretty;
};

}  // namespace uoconvert

namespace uoconvert
{
// UO's fixed-width ASCII fields are really Latin-1; JSON needs UTF-8. Stops at the first NUL.
std::string latin1ToUtf8(std::string_view s);
}  // namespace uoconvert
