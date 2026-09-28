// SPDX-License-Identifier: BSD-2-Clause
// Part of the AxmolUO client core. Ported from ClassicUO.Assets (SoundsLoader).
//
// Engine-free port of ClassicUO's SoundsLoader: sound effects from the archive (with Sound.def
// aliases and the shard's loose Sounds/<id>.wav overrides) and the music table from Config.txt.
//
// Nothing here touches Axmol. The client's audio manager (uo/client/audio) turns what this returns
// into files AudioEngine can play.

#pragma once

#include "uo/io/ClientVersion.h"
#include "uo/io/UOFile.h"

#include <cstdint>
#include <filesystem>
#include <istream>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace uo::sound
{

struct SoundEffect
{
    std::string name;  // the 40-byte header's name, or the override's file stem

    // Exactly one of these is set. `pcm` is archive data (22,050 Hz mono 16-bit, headerless);
    // `file` is a loose override the platform decoder can open directly.
    std::vector<uint8_t> pcm;
    std::filesystem::path file;

    // How long the same id is held off from replaying, as ClassicUO does.
    uint32_t replayDelayMs = 0;
};

struct MusicTrack
{
    std::string name;            // as Config.txt spells it, without extension
    std::filesystem::path file;  // resolved on disk, case-insensitively; empty if not found
    bool loop = false;
};

class SoundLoader
{
public:
    static constexpr int kMaxSoundId = 0xFFFF;
    static constexpr int kMaxMusicId = 150;  // Constants.MAX_MUSIC_DATA_INDEX_COUNT

    // UOP path pattern of sound entries, as every UOP client names them.
    static constexpr const char* kUopPattern = "build/soundlegacymul/%08u.dat";

    struct Options
    {
        std::filesystem::path uoPath;  // the UO install directory

        // uoconvert's output root (its --out), if any. Music is looked up in its Music/ first, so
        // MIDI-only installs play the .ogg renders the converter writes there.
        std::filesystem::path assetsPath;

        // Kept for callers; music no longer depends on it. Music/Digital/Config.txt is read when
        // that folder has one, else Music/Config.txt, whatever the version says.
        ClientVersion version = cv::CV_200;

        // Prefer soundLegacyMUL.uop when present (a UOP install). Off forces sound.mul.
        bool preferUop = true;
    };

    // Returns false when no sound archive is found; the music table still loads.
    bool load(const Options& options);

    // Re-lists Sounds/<id>.* overrides (ClassicUO's LoadOurs; a shard can drop files in live).
    void loadOverrides();

    std::optional<SoundEffect> sound(int id) const;
    const MusicTrack* music(int id) const;

    const std::unordered_map<int, MusicTrack>& musicTable() const { return _music; }
    size_t overrideCount() const { return _overrides.size(); }

    // Parsers, exposed for tests.
    static bool parseMusicConfigLine(std::string_view line, int& index, std::string& name, bool& loop);
    void applySoundDef(std::istream& def);

private:
    const io::FileIndex* entry(int id) const;
    void loadMusic();
    void indexMusicFiles(const std::filesystem::path& dir);

    Options _options;
    std::unique_ptr<io::UOFile> _archive;
    std::unordered_map<int, io::FileIndex> _aliases;  // Sound.def; an invalid index clears the id
    std::unordered_map<int, std::filesystem::path> _overrides;
    std::unordered_map<int, MusicTrack> _music;
    std::unordered_map<std::string, std::filesystem::path> _musicFiles;  // lower-case stem -> path
};

// The login screen track for a client version, as ClassicUO picks it:
// 7.0.0.0+ -> 78 (LoginLoop), after 3.0.8z -> 0, otherwise 8 (stones2).
int loginMusicIndex(ClientVersion version);

inline constexpr int kDeathMusicIndex = 42;

}  // namespace uo::sound
