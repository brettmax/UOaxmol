// SPDX-License-Identifier: BSD-2-Clause
// Part of the AxmolUO client core. Ported from ClassicUO.Assets (UOFileManager).
#pragma once

#include "uo/assets/Art.h"
#include "uo/assets/Cliloc.h"
#include "uo/assets/Gumps.h"
#include "uo/assets/Hues.h"
#include "uo/assets/Map.h"
#include "uo/assets/TileData.h"
#include "uo/io/ClientVersion.h"
#include "uo/sound/SoundLoader.h"

#include <memory>
#include <string>
#include <vector>

namespace uo::assets
{

// A UO data directory and everything the client has loaded from it. Prefers UOP archives
// when present, falling back to the classic .mul + idx pairs (which is all a T2A install has).
class Installation
{
public:
    struct Options
    {
        std::string directory;
        ClientVersion version = cv::CV_200;
        // uoconvert's output root, if converted assets exist; music is found there first.
        std::string assetsDirectory;
        std::string language  = "enu";
        std::vector<int> maps = {0};
    };

    // Loads every asset family. Returns false and fills error() on the first one missing.
    bool load(const Options& options);

    const std::string& error() const { return _error; }
    const Options& options() const { return _options; }
    bool isUop() const { return _isUop; }

    // Case-insensitive lookup: UO installs mix "Art.mul" and "art.mul" and Linux cares.
    std::string path(const std::string& name) const;
    bool exists(const std::string& name) const;

    const Hues& hues() const { return _hues; }
    const TileData& tileData() const { return _tileData; }
    const Art& art() const { return *_art; }
    const Gumps& gumps() const { return *_gumps; }
    const Cliloc& cliloc() const { return _cliloc; }

    // Sound effects and the music table. Optional: soundsLoaded() is false when the install has
    // no sound archive, and music still resolves.
    sound::SoundLoader& sounds() { return _sounds; }
    const sound::SoundLoader& sounds() const { return _sounds; }
    bool soundsLoaded() const { return _soundsLoaded; }
    const MapFacet* map(int index) const;

private:
    std::unique_ptr<io::UOFile> openIndexed(const std::string& uopName, const std::string& uopPattern,
                                            const std::string& mul, const std::string& idx, bool hasExtra);

    Options _options;
    std::string _error;
    bool _isUop = false;

    Hues _hues;
    TileData _tileData;
    std::unique_ptr<Art> _art;
    std::unique_ptr<Gumps> _gumps;
    Cliloc _cliloc;
    sound::SoundLoader _sounds;
    bool _soundsLoaded = false;
    std::vector<std::unique_ptr<MapFacet>> _maps;
};

}  // namespace uo::assets
