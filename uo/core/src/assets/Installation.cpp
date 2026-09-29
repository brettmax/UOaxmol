// SPDX-License-Identifier: BSD-2-Clause
#include "uo/assets/Installation.h"

#include <algorithm>
#include <cctype>
#include <filesystem>

namespace fs = std::filesystem;

namespace uo::assets
{

namespace
{
std::string lower(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
    return s;
}
}  // namespace

std::string Installation::path(const std::string& name) const
{
    fs::path direct = fs::path(_options.directory) / name;
    std::error_code ec;
    if (fs::exists(direct, ec))
        return direct.string();

    std::string want = lower(name);
    for (const auto& entry : fs::directory_iterator(_options.directory, ec))
    {
        if (lower(entry.path().filename().string()) == want)
            return entry.path().string();
    }
    return direct.string();
}

bool Installation::exists(const std::string& name) const
{
    std::error_code ec;
    return fs::exists(path(name), ec);
}

std::unique_ptr<io::UOFile> Installation::openIndexed(const std::string& uopName, const std::string& uopPattern,
                                                      const std::string& mul, const std::string& idx, bool hasExtra)
{
    std::unique_ptr<io::UOFile> file;
    if (!uopName.empty() && exists(uopName))
    {
        file   = std::make_unique<io::UopFile>(path(uopName), uopPattern, hasExtra);
        _isUop = true;
    }
    else
    {
        file = std::make_unique<io::MulFile>(path(mul), path(idx));
    }
    if (!file->load())
        return nullptr;
    return file;
}

bool Installation::load(const Options& options)
{
    _options = options;
    _error.clear();
    _isUop = false;

    std::error_code ec;
    if (!fs::is_directory(options.directory, ec))
    {
        _error = "UO data directory not found: " + options.directory;
        return false;
    }

    if (!_hues.load(path("hues.mul"), path("radarcol.mul")))
    {
        _error = "hues.mul is missing";
        return false;
    }

    // The record layout belongs to the file, not to the protocol version: Second Age data run
    // with the default 7.0.15.1 version still has 32-bit flags, and reading it as 64-bit
    // shifts every texId and flag (no stretched hills, random roofs over the player). The
    // version only settles a size both layouts fit.
    const auto tiledataSize = fs::file_size(path("tiledata.mul"), ec);
    const auto format       = ec ? TileData::Format::Auto
                                 : TileData::detect(static_cast<std::size_t>(tiledataSize),
                                                    options.version >= cv::CV_7090 ? TileData::Format::New
                                                                                   : TileData::Format::Old);
    if (!_tileData.load(path("tiledata.mul"), format))
    {
        _error = "tiledata.mul is missing or malformed";
        return false;
    }

    auto art = openIndexed("artLegacyMUL.uop", "build/artlegacymul/%08u.tga", "art.mul", "artidx.mul", false);
    if (!art)
    {
        _error = "art.mul/artidx.mul (or artLegacyMUL.uop) is missing";
        return false;
    }
    _art = std::make_unique<Art>(std::move(art));

    auto gumps = openIndexed("gumpartLegacyMUL.uop", "build/gumpartlegacymul/%08u.tga", "gumpart.mul",
                             "gumpidx.mul", true);
    if (!gumps)
    {
        _error = "gumpart.mul/gumpidx.mul (or gumpartLegacyMUL.uop) is missing";
        return false;
    }
    _gumps = std::make_unique<Gumps>(std::move(gumps), &_hues);

    // Multis are survivable too: without them houses and boats are not drawn. Records grew from
    // 12 to 16 bytes with the 64-bit tile flags, so the tiledata layout decides which this is.
    _multis.reset();
    if (exists("multi.mul") && exists("multi.idx"))
    {
        auto multi = std::make_unique<io::MulFile>(path("multi.mul"), path("multi.idx"));
        if (multi->load())
            _multis = std::make_unique<MultiLoader>(std::move(multi), _tileData.format() == TileData::Format::New);
    }

    // A missing cliloc is survivable: pre-AOS shards send plain text.
    _cliloc.load(*this, options.language);

    // Sounds are survivable too: the client plays silence rather than refusing to start.
    sound::SoundLoader::Options soundOptions;
    soundOptions.uoPath     = options.directory;
    soundOptions.assetsPath = options.assetsDirectory;
    soundOptions.version    = options.version;
    _soundsLoaded        = _sounds.load(soundOptions);

    _maps.clear();
    for (int m : options.maps)
    {
        if (m < 0)
            continue;
        if (static_cast<std::size_t>(m) >= _maps.size())
            _maps.resize(m + 1);
        auto facet = std::make_unique<MapFacet>();
        if (facet->load(options.directory, m))
            _maps[m] = std::move(facet);
    }
    if (_maps.empty() || !_maps[options.maps.empty() ? 0 : options.maps.front()])
    {
        _error = "map" + std::to_string(options.maps.empty() ? 0 : options.maps.front()) + ".mul is missing";
        return false;
    }
    return true;
}

const MapFacet* Installation::map(int index) const
{
    if (index < 0 || static_cast<std::size_t>(index) >= _maps.size())
        return nullptr;
    return _maps[index].get();
}

}  // namespace uo::assets
