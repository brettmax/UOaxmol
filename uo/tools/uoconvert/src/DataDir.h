// SPDX-License-Identifier: BSD-2-Clause
#pragma once

#include <string>
#include <unordered_map>
#include <vector>

namespace uoconvert
{

// Case-insensitive index of every file under a UO data folder. UO installs mix "Art.mul",
// "art.mul" and "Music/Digital/..." and Linux file systems care.
class DataDir
{
public:
    bool open(const std::string& root);

    const std::string& root() const { return _root; }
    // Absolute path of `relative` (any case, '/' separators), or "" when absent.
    std::string find(const std::string& relative) const;
    bool has(const std::string& relative) const { return !find(relative).empty(); }
    // Relative paths (original case) of every file under `folder` with extension `ext`.
    std::vector<std::string> list(const std::string& folder, const std::string& ext) const;

private:
    std::string _root;
    std::unordered_map<std::string, std::string> _files;  // lower-case relative -> original relative
};

std::string toLower(std::string s);

}  // namespace uoconvert
