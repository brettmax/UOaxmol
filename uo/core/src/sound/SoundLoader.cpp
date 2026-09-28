// SPDX-License-Identifier: BSD-2-Clause
// Part of the AxmolUO client core. Ported from ClassicUO.Assets (SoundsLoader).
#include "uo/sound/SoundLoader.h"

#include "uo/sound/Wave.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <cstring>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

namespace uo::sound
{

namespace
{

constexpr size_t kSoundHeaderBytes = 40;

std::string lower(std::string_view s)
{
    std::string out(s);
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) { return std::tolower(c); });
    return out;
}

// UO installs are inconsistent about case ("Sound.def", "sound.def", "SOUNDIDX.MUL"). Finds `name`
// directly under `dir` ignoring case, which matters on case-sensitive filesystems.
fs::path findFile(const fs::path& dir, std::string_view name)
{
    fs::path direct = dir / name;
    std::error_code ec;

    if (fs::is_regular_file(direct, ec))
    {
        return direct;
    }

    const std::string want = lower(name);

    for (const auto& e : fs::directory_iterator(dir, ec))
    {
        if (e.is_regular_file(ec) && lower(e.path().filename().string()) == want)
        {
            return e.path();
        }
    }

    return {};
}

fs::path findDir(const fs::path& dir, std::string_view name)
{
    std::error_code ec;
    const std::string want = lower(name);

    for (const auto& e : fs::directory_iterator(dir, ec))
    {
        if (e.is_directory(ec) && lower(e.path().filename().string()) == want)
        {
            return e.path();
        }
    }

    return {};
}

bool parseInt(std::string_view token, int& out)
{
    // DefReader.SanitizeStringNumber: keep the leading run of digits and signs.
    int base = 10;

    if (token.size() > 2 && token[0] == '0' && (token[1] == 'x' || token[1] == 'X'))
    {
        token.remove_prefix(2);
        base = 16;
    }

    size_t n = 0;

    while (n < token.size() && (std::isxdigit(static_cast<unsigned char>(token[n])) || token[n] == '-' || token[n] == '+'))
    {
        if (base == 10 && std::isalpha(static_cast<unsigned char>(token[n])))
        {
            break;
        }
        ++n;
    }

    if (n > 0 && token[0] == '+')
    {
        token.remove_prefix(1);
        --n;
    }

    auto [ptr, err] = std::from_chars(token.data(), token.data() + n, out, base);
    return err == std::errc{} && ptr != token.data();
}

void trim(std::string_view& s)
{
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front())))
    {
        s.remove_prefix(1);
    }

    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back())))
    {
        s.remove_suffix(1);
    }
}

// Default table ClassicUO falls back to when no Config.txt is present.
struct DefaultTrack
{
    const char* name;
    bool loop;
};

constexpr std::array<DefaultTrack, 67> kDefaultMusic = {{
    {"oldult01", true},       {"create1", false},        {"dragflit", false},       {"oldult02", true},
    {"oldult03", true},       {"oldult04", true},        {"oldult05", true},        {"oldult06", true},
    {"stones2", true},        {"britain1", true},        {"britain2", true},        {"bucsden", true},
    {"jhelom", false},        {"lbcastle", false},       {"linelle", false},        {"magincia", true},
    {"minoc", true},          {"ocllo", true},           {"samlethe", false},       {"serpents", true},
    {"skarabra", true},       {"trinsic", true},         {"vesper", true},          {"wind", true},
    {"yew", true},            {"cave01", false},         {"dungeon9", false},       {"forest_a", false},
    {"intown01", false},      {"jungle_a", false},       {"mountn_a", false},       {"plains_a", false},
    {"sailing", false},       {"swamp_a", false},        {"tavern01", false},       {"tavern02", false},
    {"tavern03", false},      {"tavern04", false},       {"combat1", false},        {"combat2", false},
    {"combat3", false},       {"approach", false},       {"death", false},          {"victory", false},
    {"btcastle", false},      {"nujelm", true},          {"dungeon2", false},       {"cove", true},
    {"moonglow", true},       {"zento", true},           {"tokunodungeon", true},   {"Taiko", true},
    {"dreadhornarea", true},  {"elfcity", true},         {"grizzledungeon", true},  {"melisandeslair", true},
    {"paroxysmuslair", true}, {"gwennoconversation", true}, {"goodendgame", true},  {"goodvsevil", true},
    {"greatearthserpents", true}, {"humanoids_u9", true}, {"minocnegative", true},  {"paws", true},
    {"selimsbar", true},      {"serpentislecombat_u7", true}, {"valoriaships", true},
}};

// Playable formats in order of preference. .mid is what pre-3.0.6e clients (T2A included)
// shipped; AudioEngine has no MIDI decoder, so those are expected to be pre-rendered to .ogg by
// the asset pipeline. A track that only has a .mid is left unresolved.
constexpr std::array<std::string_view, 3> kMusicExtensions = {".mp3", ".ogg", ".wav"};

}  // namespace

const io::FileIndex* SoundLoader::entry(int id) const
{
    if (id < 0 || id >= kMaxSoundId || !_archive)
    {
        return nullptr;
    }

    if (auto it = _aliases.find(id); it != _aliases.end())
    {
        return it->second.valid() ? &it->second : nullptr;
    }

    const io::FileIndex* e = _archive->entry(static_cast<size_t>(id));

    // Sound entries ship uncompressed. A compressed one would play as noise, so it is left out
    // rather than handed to the mixer.
    return e && e->compression == io::CompressionType::None ? e : nullptr;
}

bool SoundLoader::load(const Options& options)
{
    _options = options;
    const fs::path& base = options.uoPath;

    _archive.reset();
    _aliases.clear();

    if (options.preferUop)
    {
        if (fs::path uop = findFile(base, "soundLegacyMUL.uop"); !uop.empty())
        {
            _archive = std::make_unique<io::UopFile>(uop.string(), kUopPattern);
        }
    }

    if (!_archive)
    {
        fs::path mul = findFile(base, "sound.mul");
        fs::path idx = findFile(base, "soundidx.mul");

        if (!mul.empty() && !idx.empty())
        {
            _archive = std::make_unique<io::MulFile>(mul.string(), idx.string());
        }
    }

    if (_archive && !_archive->load())
    {
        _archive.reset();
    }

    const bool opened = _archive != nullptr;

    loadOverrides();

    if (opened)
    {
        if (fs::path def = findFile(base, "Sound.def"); !def.empty())
        {
            std::ifstream in(def);
            applySoundDef(in);
        }
    }

    loadMusic();

    return opened;
}

void SoundLoader::applySoundDef(std::istream& def)
{
    std::string raw;

    while (std::getline(def, raw))
    {
        std::string_view line = raw;
        trim(line);

        if (line.empty() || line[0] == '#' || !std::isdigit(static_cast<unsigned char>(line[0])))
        {
            continue;
        }

        if (size_t hash = line.find('#'); hash != std::string_view::npos)
        {
            line = line.substr(0, hash);
        }

        const size_t open  = line.find('{');
        const size_t close = line.find('}', open == std::string_view::npos ? 0 : open);

        if (open == std::string_view::npos || close == std::string_view::npos)
        {
            continue;
        }

        int index = -1;

        if (!parseInt(line.substr(0, open), index))
        {
            continue;
        }

        // Sound.def only fills holes: an id that already has data is never replaced.
        if (index < 0 || index >= kMaxSoundId || entry(index))
        {
            continue;
        }

        std::string_view group = line.substr(open + 1, close - open - 1);

        // Every member is tried in order and a later valid one wins, exactly as ClassicUO's loop
        // does (it does not break on the first hit). -1 clears the id.
        size_t at = 0;

        while (at < group.size())
        {
            size_t end = group.find_first_of(", \t", at);

            if (end == std::string_view::npos)
            {
                end = group.size();
            }

            std::string_view token = group.substr(at, end - at);
            at                     = end + 1;

            int source = 0;

            if (token.empty() || !parseInt(token, source) || source < -1 || source >= kMaxSoundId)
            {
                continue;
            }

            if (source == -1)
            {
                _aliases[index] = io::FileIndex{-1, 0};
            }
            else if (const io::FileIndex* e = entry(source))
            {
                _aliases[index] = *e;
            }
        }
    }
}

void SoundLoader::loadOverrides()
{
    _overrides.clear();

    fs::path folder = findDir(_options.uoPath, "Sounds");

    if (folder.empty())
    {
        return;
    }

    std::error_code ec;

    for (const auto& e : fs::directory_iterator(folder, ec))
    {
        if (!e.is_regular_file(ec))
        {
            continue;
        }

        const std::string ext = lower(e.path().extension().string());

        // ClassicUO accepts only 22,050 Hz mono 16-bit WAV because it streams raw PCM into a
        // fixed-format voice. AudioEngine decodes and resamples, so any WAV, OGG or MP3 works.
        if (ext != ".wav" && ext != ".ogg" && ext != ".mp3")
        {
            continue;
        }

        int id = -1;
        const std::string stem = e.path().stem().string();
        auto [ptr, err]        = std::from_chars(stem.data(), stem.data() + stem.size(), id);

        if (err == std::errc{} && ptr == stem.data() + stem.size() && id >= 0 && id < kMaxSoundId)
        {
            _overrides[id] = e.path();
        }
    }
}

std::optional<SoundEffect> SoundLoader::sound(int id) const
{
    if (id < 0 || id >= kMaxSoundId)
    {
        return std::nullopt;
    }

    // Overrides first, so a replacement replaces and a new id works at all.
    if (auto it = _overrides.find(id); it != _overrides.end())
    {
        SoundEffect fx;
        fx.name = it->second.stem().string();
        fx.file = it->second;

        // The replay throttle is derived from the sample count, so read the header of a WAV.
        if (lower(it->second.extension().string()) == ".wav")
        {
            std::ifstream in(it->second, std::ios::binary);
            std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
            WaveFormat fmt;
            std::span<const uint8_t> data;

            if (!parseWave(bytes, fmt, data))
            {
                return std::nullopt;
            }

            // Scale to the UO format's byte rate so the throttle means the same thing.
            const double seconds = static_cast<double>(data.size()) /
                                   std::max(1u, fmt.sampleRate * fmt.channels * (fmt.bitsPerSample / 8u));
            fx.replayDelayMs = uoSoundDelayMs(static_cast<size_t>(seconds * kUoSampleRate * 2));
        }

        return fx;
    }

    const io::FileIndex* e = entry(id);

    if (!e)
    {
        return std::nullopt;
    }

    std::span<const uint8_t> raw = _archive->raw(*e);

    if (raw.size() <= kSoundHeaderBytes)
    {
        return std::nullopt;
    }

    SoundEffect fx;
    const char* header = reinterpret_cast<const char*>(raw.data());
    fx.name.assign(header, strnlen(header, kSoundHeaderBytes));
    fx.pcm.assign(raw.begin() + kSoundHeaderBytes, raw.end());
    fx.replayDelayMs = uoSoundDelayMs(fx.pcm.size());

    return fx;
}

bool SoundLoader::parseMusicConfigLine(std::string_view line, int& index, std::string& name, bool& loop)
{
    // ClassicUO splits on ' ', ',' and '\t' without dropping empties and wants 2 or 3 fields:
    // "<index> <name>[,loop]" or "<index> <name> loop".
    while (!line.empty() && (line.back() == '\r' || line.back() == '\n'))
    {
        line.remove_suffix(1);
    }

    std::array<std::string_view, 3> parts;
    size_t count = 0;
    size_t at    = 0;

    while (true)
    {
        size_t end = line.find_first_of(" ,\t", at);

        if (count == parts.size())
        {
            return false;
        }

        parts[count++] = line.substr(at, end == std::string_view::npos ? std::string_view::npos : end - at);

        if (end == std::string_view::npos)
        {
            break;
        }

        at = end + 1;
    }

    if (count < 2)
    {
        return false;
    }

    auto [ptr, err] = std::from_chars(parts[0].data(), parts[0].data() + parts[0].size(), index);

    if (err != std::errc{} || ptr != parts[0].data() + parts[0].size())
    {
        return false;
    }

    std::string_view n = parts[1];

    if (size_t dot = n.rfind('.'); dot != std::string_view::npos)
    {
        n = n.substr(0, dot);
    }

    name = std::string(n);
    loop = count == 3 && parts[2] == "loop";

    return !name.empty();
}

void SoundLoader::indexMusicFiles(const fs::path& dir)
{
    std::error_code ec;

    for (auto it = fs::recursive_directory_iterator(dir, ec); it != fs::recursive_directory_iterator(); it.increment(ec))
    {
        if (ec || !it->is_regular_file(ec))
        {
            continue;
        }

        const std::string ext = lower(it->path().extension().string());
        auto rank             = std::find(kMusicExtensions.begin(), kMusicExtensions.end(), ext);

        if (rank == kMusicExtensions.end())
        {
            continue;
        }

        const std::string key = lower(it->path().stem().string());
        auto existing         = _musicFiles.find(key);

        if (existing == _musicFiles.end())
        {
            _musicFiles.emplace(key, it->path());
            continue;
        }

        auto existingRank =
            std::find(kMusicExtensions.begin(), kMusicExtensions.end(), lower(existing->second.extension().string()));

        if (rank < existingRank)
        {
            existing->second = it->path();
        }
    }
}

void SoundLoader::loadMusic()
{
    _music.clear();
    _musicFiles.clear();

    // ClientVersion.CV_4011C: the Music/ -> Music/Digital/ switchover.
    const bool useDigital = _options.version >= makeVersion(4, 0, 11, 'c');

    struct MusicRoot
    {
        fs::path music;
        fs::path digital;
    };

    auto root = [](const fs::path& base) {
        MusicRoot r;
        r.music   = base.empty() ? fs::path{} : findDir(base, "Music");
        r.digital = r.music.empty() ? fs::path{} : findDir(r.music, "Digital");
        return r;
    };

    // uoconvert's tree comes first: it mirrors Music/ and holds the .ogg renders of MIDI-only
    // installs, which it may not write into the UO folder. The UO folder is the fallback.
    const MusicRoot converted = root(_options.assetsPath);
    const MusicRoot install   = root(_options.uoPath);

    for (const MusicRoot* r : {&converted, &install})
    {
        // Digital tracks shadow same-named ones elsewhere in Music/, matching ClassicUO's lookup,
        // which builds Music/Digital/<name> whenever that folder exists.
        if (!r->digital.empty())
        {
            indexMusicFiles(r->digital);
        }

        if (!r->music.empty())
        {
            indexMusicFiles(r->music);
        }
    }

    // Config.txt from the install, else the converter's copy of it.
    fs::path config;

    for (const MusicRoot* r : {&install, &converted})
    {
        const fs::path& dir = useDigital ? r->digital : r->music;

        if (!dir.empty())
        {
            config = findFile(dir, "Config.txt");
        }

        if (!config.empty())
        {
            break;
        }
    }

    auto add = [&](int index, std::string name, bool loop) {
        MusicTrack track;
        track.loop = loop;

        if (auto f = _musicFiles.find(lower(name)); f != _musicFiles.end())
        {
            track.file = f->second;
        }

        track.name    = std::move(name);
        _music[index] = std::move(track);
    };

    if (!config.empty())
    {
        std::ifstream in(config);
        std::string line;

        while (std::getline(in, line))
        {
            int index = 0;
            std::string name;
            bool loop = false;

            if (parseMusicConfigLine(line, index, name, loop))
            {
                add(index, std::move(name), loop);
            }
        }

        return;
    }

    for (size_t i = 0; i < kDefaultMusic.size(); ++i)
    {
        add(static_cast<int>(i), kDefaultMusic[i].name, kDefaultMusic[i].loop);
    }
}

const MusicTrack* SoundLoader::music(int id) const
{
    auto it = _music.find(id);
    return it != _music.end() ? &it->second : nullptr;
}

int loginMusicIndex(ClientVersion version)
{
    if (version >= cv::CV_7000)
    {
        return 78;  // LoginLoop
    }

    if (version > makeVersion(3, 0, 8, 'z'))  // CV_308Z
    {
        return 0;
    }

    return 8;  // stones2
}

}  // namespace uo::sound
