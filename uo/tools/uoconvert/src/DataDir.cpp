// SPDX-License-Identifier: BSD-2-Clause
#include "DataDir.h"

#include <algorithm>
#include <cctype>
#include <filesystem>

namespace fs = std::filesystem;

namespace uoconvert
{

std::string toLower(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
    return s;
}

bool DataDir::open(const std::string& root)
{
    std::error_code ec;
    if (!fs::is_directory(root, ec))
        return false;
    _root = fs::absolute(root, ec).lexically_normal().string();
    _files.clear();
    for (auto it = fs::recursive_directory_iterator(_root, fs::directory_options::skip_permission_denied, ec);
         it != fs::recursive_directory_iterator(); it.increment(ec))
    {
        if (ec)
            break;
        if (!it->is_regular_file(ec))
            continue;
        std::string rel = fs::relative(it->path(), _root, ec).generic_string();
        _files.emplace(toLower(rel), rel);
    }
    return true;
}

std::string DataDir::find(const std::string& relative) const
{
    std::string key = relative;
    std::replace(key.begin(), key.end(), '\\', '/');
    auto it = _files.find(toLower(key));
    return it == _files.end() ? std::string() : (fs::path(_root) / it->second).string();
}

std::vector<std::string> DataDir::list(const std::string& folder, const std::string& ext) const
{
    std::string prefix = toLower(folder);
    if (!prefix.empty() && prefix.back() != '/')
        prefix += '/';
    std::string want = toLower(ext);
    std::vector<std::string> out;
    for (const auto& [lower, original] : _files)
    {
        if (lower.compare(0, prefix.size(), prefix) != 0)
            continue;
        if (lower.size() >= want.size() && lower.compare(lower.size() - want.size(), want.size(), want) == 0)
            out.push_back(original);
    }
    std::sort(out.begin(), out.end());
    return out;
}

}  // namespace uoconvert
