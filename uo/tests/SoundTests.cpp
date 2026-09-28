// SPDX-License-Identifier: BSD-2-Clause
// Sound loader tests over synthetic MUL/UOP archives, Sound.def, Config.txt and packets.
#include "doctest.h"

#include "uo/io/UOFile.h"
#include "uo/sound/SoundLoader.h"
#include "uo/sound/SoundPackets.h"
#include "uo/sound/Wave.h"

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace uo::sound;
using uo::io::UopFile;

namespace
{

template <typename T>
void put(std::vector<uint8_t>& out, T v)
{
    const auto* p = reinterpret_cast<const uint8_t*>(&v);
    out.insert(out.end(), p, p + sizeof(T));
}

void writeFile(const fs::path& path, const std::vector<uint8_t>& bytes)
{
    fs::create_directories(path.parent_path());
    std::ofstream(path, std::ios::binary).write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
}

void writeText(const fs::path& path, const std::string& text)
{
    fs::create_directories(path.parent_path());
    std::ofstream(path) << text;
}

// A sound entry: 40-byte name header then PCM.
std::vector<uint8_t> soundEntry(const char* name, size_t pcmBytes, uint8_t fill)
{
    std::vector<uint8_t> e(40, 0);
    std::memcpy(e.data(), name, std::strlen(name));
    e.insert(e.end(), pcmBytes, fill);
    return e;
}

SoundLoader::Options opts(const fs::path& path, uo::ClientVersion version = uo::cv::CV_200)
{
    SoundLoader::Options o;
    o.uoPath  = path;
    o.version = version;
    return o;
}

struct TempDir
{
    fs::path path;

    explicit TempDir(const char* name)
    {
        path = fs::temp_directory_path() / name;
        fs::remove_all(path);
        fs::create_directories(path);
    }

    ~TempDir() { fs::remove_all(path); }
};

void writeMul(const fs::path& dir, const std::vector<std::vector<uint8_t>>& entries)
{
    std::vector<uint8_t> mul, idx;

    for (const auto& e : entries)
    {
        if (e.empty())
        {
            put<uint32_t>(idx, 0xFFFFFFFF);
            put<int32_t>(idx, -1);
        }
        else
        {
            put<uint32_t>(idx, static_cast<uint32_t>(mul.size()));
            put<int32_t>(idx, static_cast<int32_t>(e.size()));
            mul.insert(mul.end(), e.begin(), e.end());
        }
        put<int32_t>(idx, 0);
    }

    writeFile(dir / "sound.mul", mul);
    writeFile(dir / "soundidx.mul", idx);
}

}  // namespace

TEST_CASE("WAV headers encode and parse")
{
    std::vector<uint8_t> pcm(1000, 7);
    auto wav = encodeWave(pcm);
    CHECK(wav.size() == 1044);
    CHECK(std::memcmp(wav.data(), "RIFF", 4) == 0);

    WaveFormat fmt;
    std::span<const uint8_t> data;
    CHECK(parseWave(wav, fmt, data));
    CHECK((fmt.sampleRate == 22050 && fmt.channels == 1 && fmt.bitsPerSample == 16));
    CHECK((data.size() == 1000 && data[0] == 7));

    // A LIST chunk before data, odd-sized so the pad byte matters.
    std::vector<uint8_t> withList(wav.begin(), wav.begin() + 36);
    withList.insert(withList.end(), {'L', 'I', 'S', 'T', 3, 0, 0, 0, 'a', 'b', 'c', 0});
    withList.insert(withList.end(), wav.begin() + 36, wav.end());
    CHECK(parseWave(withList, fmt, data));
    CHECK(data.size() == 1000);

    std::vector<uint8_t> junk(64, 0);
    CHECK(!parseWave(junk, fmt, data));

    CHECK(uoSoundDelayMs(8852) == 100);  // (8852 - 32) / 88.2
    CHECK(uoSoundDelayMs(10) == 0);
}

TEST_CASE("UOP hash matches ClassicUO CreateHash")
{
    // Reference values from a line-by-line Python transcription of ClassicUO's CreateHash.
    CHECK(UopFile::hash("build/soundlegacymul/00000000.dat") == 0xCB36450C320CD308ull);
    CHECK(UopFile::hash("build/soundlegacymul/00000042.dat") == 0x6D5E47C6C50CCDC2ull);
    CHECK(UopFile::hash("abc") == 0x3C03BE9E0E397631ull);
    CHECK(UopFile::hash("123456789012") == 0x27F9A1C5B578F5B0ull);
}

TEST_CASE("sound.mul entries and Sound.def aliases")
{
    TempDir dir("uo_sound_mul");

    writeMul(dir.path, {
                           soundEntry("zero.wav", 100, 1),
                           {},
                           soundEntry("two.wav", 300, 2),
                           {},
                       });

    // 1 <- 2; 3 <- 0 then 2 (a later valid member wins); 0 already has data so is left alone.
    writeText(dir.path / "Sound.def", "# comment\n1 {2}\n3 {0, 2} # trailing\n0 {2}\nx {2}\n");

    SoundLoader loader;
    CHECK(loader.load(opts(dir.path)));

    auto zero = loader.sound(0);
    CHECK((zero && zero->name == "zero.wav" && zero->pcm.size() == 100 && zero->pcm[0] == 1));

    auto one = loader.sound(1);
    CHECK((one && one->name == "two.wav" && one->pcm.size() == 300));

    auto three = loader.sound(3);
    CHECK((three && three->name == "two.wav"));

    CHECK(!loader.sound(4));
    CHECK(!loader.sound(-1));
    CHECK(!loader.sound(0xFFFF));
}

TEST_CASE("Sound.def -1 clears an id")
{
    TempDir dir("uo_sound_def_clear");
    writeMul(dir.path, {soundEntry("a", 50, 1), {}});

    SoundLoader loader;
    CHECK(loader.load(opts(dir.path)));

    std::istringstream def("1 {0, -1}\n");
    loader.applySoundDef(def);
    CHECK(!loader.sound(1));
}

TEST_CASE("soundLegacyMUL.uop entries, compressed ones skipped")
{
    TempDir dir("uo_sound_uop");

    const auto a = soundEntry("uop_a", 120, 9);
    const auto b = soundEntry("uop_b", 60, 8);

    // Header (28 bytes) then one block of 3 entries; the third is compressed and must be skipped.
    std::vector<uint8_t> f;
    put<uint32_t>(f, 0x50594D);
    put<uint32_t>(f, 5);
    put<uint32_t>(f, 0xFD23EC43);
    put<int64_t>(f, 28);
    put<uint32_t>(f, 100);
    put<int32_t>(f, 3);

    const int64_t tableStart = 28;
    const int64_t tableSize  = 4 + 8 + 3 * 34;
    int64_t dataAt           = tableStart + tableSize;

    auto entry = [&](int id, const std::vector<uint8_t>& data, int16_t flag) {
        char name[40];
        std::snprintf(name, sizeof(name), SoundLoader::kUopPattern, static_cast<unsigned>(id));
        put<int64_t>(f, dataAt);
        put<int32_t>(f, 0);  // header length
        put<int32_t>(f, static_cast<int32_t>(data.size()));
        put<int32_t>(f, static_cast<int32_t>(data.size()));
        put<uint64_t>(f, UopFile::hash(name));
        put<uint32_t>(f, 0);
        put<int16_t>(f, flag);
        dataAt += static_cast<int64_t>(data.size());
    };

    put<int32_t>(f, 3);
    put<int64_t>(f, 0);
    entry(5, a, 0);
    entry(700, b, 0);
    entry(6, b, 1);
    f.insert(f.end(), a.begin(), a.end());
    f.insert(f.end(), b.begin(), b.end());
    f.insert(f.end(), b.begin(), b.end());

    writeFile(dir.path / "soundLegacyMUL.uop", f);

    SoundLoader loader;
    CHECK(loader.load(opts(dir.path)));

    auto five = loader.sound(5);
    CHECK((five && five->name == "uop_a" && five->pcm.size() == 120 && five->pcm[119] == 9));

    auto big = loader.sound(700);
    CHECK((big && big->name == "uop_b" && big->pcm.size() == 60));

    CHECK(!loader.sound(6));
    CHECK(!loader.sound(7));
}

TEST_CASE("Sounds/ overrides win and add ids")
{
    TempDir dir("uo_sound_overrides");
    writeMul(dir.path, {soundEntry("archive", 100, 1)});

    // 44,100 Hz stereo: fine for AudioEngine. The throttle scales to the UO byte rate, so 100 ms
    // of audio holds off like 100 ms of archive PCM: (4410 - 32) / 88.2 = 49 ms.
    std::vector<uint8_t> pcm(44100 * 4 / 10, 0);  // 100 ms
    writeFile(dir.path / "Sounds" / "0.wav", encodeWave(pcm, {44100, 2, 16}));
    writeFile(dir.path / "Sounds" / "40000.ogg", {1, 2, 3});
    writeFile(dir.path / "Sounds" / "notanid.wav", {1});

    SoundLoader loader;
    CHECK(loader.load(opts(dir.path)));
    CHECK(loader.overrideCount() == 2);

    auto zero = loader.sound(0);
    CHECK((zero && zero->pcm.empty() && zero->file.filename() == "0.wav"));
    CHECK((zero && zero->replayDelayMs == 49));

    auto added = loader.sound(40000);
    CHECK((added && added->file.filename() == "40000.ogg"));
}

TEST_CASE("music Config.txt lines parse")
{
    int index = 0;
    std::string name;
    bool loop = false;

    CHECK(SoundLoader::parseMusicConfigLine("9 britain1,loop", index, name, loop));
    CHECK((index == 9 && name == "britain1" && loop));

    CHECK(SoundLoader::parseMusicConfigLine("12 jhelom.mp3\r", index, name, loop));
    CHECK((index == 12 && name == "jhelom" && !loop));

    CHECK(SoundLoader::parseMusicConfigLine("3\tOldUlt02 loop", index, name, loop));
    CHECK((index == 3 && name == "OldUlt02" && loop));

    CHECK(!SoundLoader::parseMusicConfigLine("", index, name, loop));
    CHECK(!SoundLoader::parseMusicConfigLine("x name", index, name, loop));
    CHECK(!SoundLoader::parseMusicConfigLine("1 a b c", index, name, loop));
}

TEST_CASE("music table resolves files case-insensitively")
{
    TempDir dir("uo_sound_music");
    writeMul(dir.path, {soundEntry("a", 10, 1)});

    writeText(dir.path / "Music" / "Digital" / "Config.txt", "0 oldult01,loop\n9 Britain1,loop\n42 death\n43 victory\n");
    writeFile(dir.path / "Music" / "Digital" / "OLDULT01.MP3", {0});
    writeFile(dir.path / "Music" / "Digital" / "britain1.mp3", {0});
    writeFile(dir.path / "Music" / "death.mid", {0});  // MIDI only: not playable
    writeFile(dir.path / "Music" / "victory.mid", {0});
    writeFile(dir.path / "Music" / "victory.ogg", {0});  // pre-rendered by the pipeline

    SoundLoader loader;
    CHECK(loader.load(opts(dir.path, uo::cv::CV_7000)));
    CHECK(loader.musicTable().size() == 4);

    auto* t0 = loader.music(0);
    CHECK((t0 && t0->loop && t0->file.filename() == "OLDULT01.MP3"));

    auto* t9 = loader.music(9);
    CHECK((t9 && t9->file.filename() == "britain1.mp3"));

    auto* death = loader.music(42);
    CHECK((death && !death->loop && death->file.empty()));

    auto* victory = loader.music(43);
    CHECK((victory && victory->file.filename() == "victory.ogg"));

    CHECK(!loader.music(1));
}

TEST_CASE("built-in music table without Config.txt")
{
    TempDir dir("uo_sound_music_default");

    SoundLoader loader;
    CHECK(!loader.load(opts(dir.path)));  // no archive, but music still loads
    CHECK(loader.musicTable().size() == 67);
    CHECK((loader.music(8) && loader.music(8)->name == "stones2" && loader.music(8)->loop));
    CHECK((loader.music(42) && loader.music(42)->name == "death"));
}

TEST_CASE("login music by client version")
{
    CHECK(loginMusicIndex(uo::makeVersion(2, 0, 0, 0)) == 8);           // T2A
    CHECK(loginMusicIndex(uo::makeVersion(3, 0, 8, 'z')) == 8);
    CHECK(loginMusicIndex(uo::makeVersion(4, 0, 0, 0)) == 0);
    CHECK(loginMusicIndex(uo::makeVersion(7, 0, 15, 1)) == 78);
}

TEST_CASE("0x54 and 0x6D payloads parse")
{
    const uint8_t sound[] = {0x01, 0x02, 0x3A, 0x00, 0x00, 0x05, 0xDC, 0x06, 0x40, 0xFF, 0xFB};
    auto s                = parsePlaySound(sound);
    CHECK((s && s->mode == 1 && s->sound == 0x023A && s->x == 1500 && s->y == 1600 && s->z == -5));
    CHECK(!parsePlaySound(std::span(sound, 5)));

    const uint8_t play[] = {0x00, 0x09};
    auto m               = parsePlayMusic(play);
    CHECK((m && !m->stop && m->music == 9));

    const uint8_t stop[] = {0x1F, 0xFF};
    auto st              = parsePlayMusic(stop);
    CHECK((st && st->stop));

    CHECK(!parsePlayMusic(std::span(play, 1)));
}

TEST_CASE("T2A clients read Music/Config.txt, not the Digital one")
{
    TempDir dir("uo_sound_music_t2a");
    writeText(dir.path / "Music" / "Config.txt", "9 britain1,loop\n");
    writeText(dir.path / "Music" / "Digital" / "Config.txt", "0 oldult01,loop\n");
    writeFile(dir.path / "Music" / "britain1.mid", {0});
    writeFile(dir.path / "Music" / "britain1.ogg", {0});

    SoundLoader loader;
    loader.load(opts(dir.path));
    CHECK(loader.musicTable().size() == 1);

    auto* t9 = loader.music(9);
    CHECK((t9 && t9->loop && t9->file.filename() == "britain1.ogg"));
}
