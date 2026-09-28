// SPDX-License-Identifier: BSD-2-Clause
// Part of the AxmolUO client core. Ported from ClassicUO.IO (DefReader): the tokenizer for the *.def tables
// (Body.def, Bodyconv.def, Corpse.def, Equipconv.def, Anim1.def, Anim2.def).
#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace uo::io
{

class DefReader
{
public:
    // Lines with fewer than minSize tokens are dropped, as in ClassicUO.
    static DefReader fromFile(const std::filesystem::path& path, int minSize = 2);
    static DefReader fromString(std::string_view text, int minSize = 2);

    bool next();
    int partsCount() const;
    int linesCount() const { return static_cast<int>(_lines.size()); }

    // Next token as an integer; -1 when it does not parse. Like ClassicUO, the token is cut
    // at its first character that is not a digit or sign before parsing, so "0x10" reads as 0.
    int readInt();

    // Element `index` of the next token, which must be a "{a, b, ...}" group.
    std::optional<int> readGroupInt(int index = 0);

    // All decimal numbers in the next "{...}" group; nullopt when the token is not a group.
    std::optional<std::vector<int>> readGroup();

private:
    const std::string& tokenAt(int line, int index) const;

    std::vector<std::vector<std::string>> _lines;
    int _line     = -1;
    int _position = 0;
};

} // namespace uo::io
