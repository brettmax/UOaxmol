// SPDX-License-Identifier: BSD-2-Clause
// Music and Spine. Neither is a UO binary format: music files are copied (mp3) or rendered
// (MIDI -> Ogg Vorbis with the built-in synthesizer, since Axmol's AudioEngine cannot play MIDI), and Spine exports
// are checked against the runtime version UOspine-axmol ships and copied.
#include "Midi.h"
#include "Stages.h"

#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <mutex>
#include <regex>
#include <sstream>
#include <thread>

namespace fs = std::filesystem;

namespace uoconvert
{

namespace
{

// ClassicUO's built-in track list, used when the client has no Music/Digital/Config.txt.
const std::map<int, std::pair<const char*, bool>>& defaultTracks()
{
    static const std::map<int, std::pair<const char*, bool>> t = {
        {0, {"oldult01", true}},   {1, {"create1", false}},   {2, {"dragflit", false}},  {3, {"oldult02", true}},
        {4, {"oldult03", true}},   {5, {"oldult04", true}},   {6, {"oldult05", true}},   {7, {"oldult06", true}},
        {8, {"stones2", true}},    {9, {"britain1", true}},   {10, {"britain2", true}},  {11, {"bucsden", true}},
        {12, {"jhelom", false}},   {13, {"lbcastle", false}}, {14, {"linelle", false}},  {15, {"magincia", true}},
        {16, {"minoc", true}},     {17, {"ocllo", true}},     {18, {"samlethe", false}}, {19, {"serpents", true}},
        {20, {"skarabra", true}},  {21, {"trinsic", true}},   {22, {"vesper", true}},    {23, {"wind", true}},
        {24, {"yew", true}},       {25, {"cave01", false}},   {26, {"dungeon9", false}}, {27, {"forest_a", false}},
        {28, {"intown01", false}}, {29, {"jungle_a", false}}, {30, {"mountn_a", false}}, {31, {"plains_a", false}},
        {32, {"sailing", false}},  {33, {"swamp_a", false}},  {34, {"tavern01", false}}, {35, {"tavern02", false}},
        {36, {"tavern03", false}}, {37, {"tavern04", false}}, {38, {"combat1", false}},  {39, {"combat2", false}},
        {40, {"combat3", false}},  {41, {"approach", false}}, {42, {"death", false}},    {43, {"victory", false}},
        {44, {"btcastle", false}}, {45, {"nujelm", true}},    {46, {"dungeon2", false}}, {47, {"cove", true}},
        {48, {"moonglow", true}},  {49, {"zento", true}},     {50, {"tokunodungeon", true}}, {51, {"Taiko", true}},
        {52, {"dreadhornarea", true}}, {53, {"elfcity", true}}, {54, {"grizzledungeon", true}},
        {55, {"melisandeslair", true}}, {56, {"paroxysmuslair", true}}, {57, {"gwennoconversation", true}},
        {58, {"goodendgame", true}}, {59, {"goodvsevil", true}}, {60, {"greatearthserpents", true}},
        {61, {"humanoids_u9", true}}, {62, {"minocnegative", true}}, {63, {"paws", true}}, {64, {"selimsbar", true}},
        {65, {"serpentislecombat_u7", true}}, {66, {"valoriaships", true}},
    };
    return t;
}

}  // namespace

bool runMusic(Context& ctx, JsonWriter& m)
{
    std::vector<std::string> files = ctx.data.list("Music", "");
    if (files.empty())
    {
        m.field("skipped", "no Music/ folder");
        return false;
    }

    // The soundfont: --soundfont, else the one the disc ships (UO:R has MUSIC/4mb/UO_4MB_2.SF2
    // and MUSIC/512K/UO_512.SF2), else the largest .sf2 under Music/.
    std::string soundfont = ctx.opt.soundfont;
    if (soundfont.empty())
        soundfont = ctx.data.find("Music/4mb/UO_4MB_2.SF2");
    if (soundfont.empty())
    {
        std::uintmax_t best = 0;
        for (const std::string& rel : files)
        {
            if (toLower(fs::path(rel).extension().string()) != ".sf2")
                continue;
            std::error_code ec;
            std::string path = (fs::path(ctx.data.root()) / rel).string();
            if (std::uintmax_t size = fs::file_size(path, ec); !ec && size > best)
                best = size, soundfont = path;
        }
    }
    MidiRenderer synth;
    bool canRender = false;
    if (!soundfont.empty())
    {
        canRender = synth.loadSoundFont(Context::readFile(soundfont));
        if (!canRender)
            ctx.warn("music: " + soundfont + " is not a readable SoundFont 2 bank");
    }

    std::size_t copied = 0, pending = 0;
    std::vector<std::pair<std::string, std::string>> midis;  // source relative path, output .ogg
    for (const std::string& rel : files)
    {
        std::string ext = toLower(fs::path(rel).extension().string());
        std::string src = (fs::path(ctx.data.root()) / rel).string();
        if (ext == ".mp3" || ext == ".ogg" || ext == ".wav" || ext == ".txt")
        {
            fs::copy_file(src, ctx.out(rel), fs::copy_options::overwrite_existing);
            ++copied;
        }
        else if (ext == ".mid" || ext == ".midi")
        {
            // Same folder and base name, .ogg: the client's music loader looks in the converted
            // tree first and tries .mp3, .ogg, .wav per track.
            if (canRender)
                midis.emplace_back(rel, ctx.out(fs::path(rel).replace_extension(".ogg").generic_string()));
            else
                ++pending;
        }
    }

    // Tracks render in parallel, each on its own copy of the synthesizer.
    std::atomic<std::size_t> next{0}, rendered{0};
    std::mutex lock;
    std::vector<std::string> failures;
    auto worker = [&] {
        for (std::size_t i; (i = next++) < midis.size();)
        {
            std::string error;
            if (synth.renderToOgg(Context::readFile((fs::path(ctx.data.root()) / midis[i].first).string()),
                                  midis[i].second, 0.5f, error))
                ++rendered;
            else
            {
                std::lock_guard g(lock);
                failures.push_back(midis[i].first + ": " + error);
            }
        }
    };
    int jobs = ctx.opt.jobs > 0 ? ctx.opt.jobs : static_cast<int>(std::max(1u, std::thread::hardware_concurrency()));
    jobs     = std::max(1, std::min<int>(jobs, static_cast<int>(midis.size())));
    std::vector<std::thread> threads;
    for (int t = 1; t < jobs && !midis.empty(); ++t)
        threads.emplace_back(worker);
    worker();
    for (auto& t : threads)
        t.join();
    std::sort(failures.begin(), failures.end());
    for (const std::string& f : failures)
        ctx.warn("music: could not render " + f);
    const std::size_t failed = failures.size();
    if (pending)
        ctx.warn(std::to_string(pending) + " MIDI file(s) not rendered: no SoundFont found; pass --soundfont <.sf2>");

    // music.json: the track table the server's 0x6D PlayMusic ids index, from Config.txt when present.
    std::map<int, std::pair<std::string, bool>> tracks;
    std::string config = ctx.data.find("Music/Digital/Config.txt");
    if (config.empty())
        config = ctx.data.find("Music/Config.txt");
    if (!config.empty())
    {
        std::ifstream f(config);
        std::string line;
        while (std::getline(f, line))
        {
            std::replace(line.begin(), line.end(), ',', ' ');
            std::istringstream ss(line);
            int id;
            std::string name, loop;
            if (ss >> id >> name)
            {
                ss >> loop;
                tracks[id] = {fs::path(name).stem().string(), loop == "loop"};
            }
        }
    }
    else
        for (const auto& [id, t] : defaultTracks())
            tracks[id] = {t.first, t.second};

    JsonWriter j(true);
    j.beginObject().field("version", 1).field("source", config.empty() ? "classicuo-defaults" : "Config.txt");
    j.key("tracks").beginObject();
    for (const auto& [id, t] : tracks)
        j.key(std::to_string(id)).beginObject().field("name", t.first).field("loop", t.second).endObject();
    j.endObject().endObject();
    j.save(ctx.out("Music/music.json"));

    m.field("copied", static_cast<std::uint64_t>(copied)).field("renderedMidi", static_cast<std::uint64_t>(rendered.load()));
    m.field("pendingMidi", static_cast<std::uint64_t>(pending)).field("failedMidi", static_cast<std::uint64_t>(failed));
    m.field("tracks", static_cast<std::uint64_t>(tracks.size()));
    return true;
}

bool runSpine(Context& ctx, JsonWriter& m)
{
    if (ctx.opt.spineDir.empty())
    {
        m.field("skipped", "no --spine folder given (UO itself has no Spine content)");
        return false;
    }
    DataDir src;
    if (!src.open(ctx.opt.spineDir))
    {
        m.field("skipped", "spine folder not found");
        return false;
    }
    // UOspine-axmol ships the 4.2 and 4.3 runtimes; other major/minor versions will not load.
    static const std::regex jsonVersion("\"spine\"\\s*:\\s*\"(\\d+\\.\\d+)");
    static const std::regex binVersion("(\\d+\\.\\d+)\\.\\d+");
    std::size_t skeletons = 0, copied = 0, rejected = 0;
    for (const std::string& rel : src.list("", ""))
    {
        std::string ext = toLower(fs::path(rel).extension().string());
        std::string path = (fs::path(src.root()) / rel).string();
        if (ext == ".json" || ext == ".skel")
        {
            std::ifstream f(path, std::ios::binary);
            std::string head(512, '\0');
            f.read(head.data(), static_cast<std::streamsize>(head.size()));
            head.resize(static_cast<std::size_t>(f.gcount()));
            std::smatch mt;
            bool found = ext == ".json" ? std::regex_search(head, mt, jsonVersion) : std::regex_search(head, mt, binVersion);
            if (!found)
                continue;  // not a skeleton (some other JSON)
            ++skeletons;
            if (mt[1] != "4.2" && mt[1] != "4.3")
            {
                ctx.warn(rel + " is Spine " + mt[1].str() + "; UOspine-axmol loads 4.2 and 4.3 only");
                ++rejected;
                continue;
            }
        }
        else if (ext != ".atlas" && ext != ".png" && ext != ".webp")
            continue;
        fs::copy_file(path, ctx.out("spine/" + rel), fs::copy_options::overwrite_existing);
        ++copied;
    }
    m.field("skeletons", static_cast<std::uint64_t>(skeletons)).field("copied", static_cast<std::uint64_t>(copied));
    m.field("rejected", static_cast<std::uint64_t>(rejected));
    return true;
}

}  // namespace uoconvert
