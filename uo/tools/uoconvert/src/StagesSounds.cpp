// SPDX-License-Identifier: BSD-2-Clause
// Sound effects. uo::sound::SoundLoader resolves every id exactly as the client does (UOP or
// sound.mul, Sound.def aliases, loose Sounds/<id>.* overrides); archive PCM is wrapped in a WAV
// header by uo::sound::encodeWave, which AudioEngine plays as is.
#include "Stages.h"

#include "uo/sound/SoundLoader.h"
#include "uo/sound/Wave.h"

#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

namespace uoconvert
{

bool runSounds(Context& ctx, JsonWriter& m)
{
    uo::sound::SoundLoader loader;
    uo::sound::SoundLoader::Options o;
    o.uoPath    = ctx.opt.uoDir;
    o.version   = ctx.opt.clientVersion;
    o.preferUop = ctx.opt.useUop;
    if (!loader.load(o))
    {
        m.field("skipped", "no soundLegacyMUL.uop or sound.mul/soundidx.mul");
        return false;
    }

    const std::string dir = ctx.outDir("sounds");
    std::size_t written = 0, copied = 0, failed = 0;
    JsonWriter j;
    j.beginObject().field("version", 1).key("sounds").beginObject();
    for (int id = 0; id <= uo::sound::SoundLoader::kMaxSoundId; ++id)
    {
        auto sound = loader.sound(id);
        if (!sound)
            continue;
        std::string file;
        if (!sound->file.empty())
        {
            // A shard override: already a file the platform decoder opens, so it is copied.
            file = std::to_string(id) + sound->file.extension().string();
            std::error_code ec;
            fs::copy_file(sound->file, fs::path(dir) / file, fs::copy_options::overwrite_existing, ec);
            if (ec)
            {
                ++failed;
                ctx.warn("sounds: could not copy " + sound->file.string() + ": " + ec.message());
                continue;
            }
            ++copied;
        }
        else
        {
            if (sound->pcm.empty())
                continue;
            file = std::to_string(id) + ".wav";
            std::vector<std::uint8_t> wav = uo::sound::encodeWave(sound->pcm);
            std::ofstream f(fs::path(dir) / file, std::ios::binary);
            f.write(reinterpret_cast<const char*>(wav.data()), static_cast<std::streamsize>(wav.size()));
            if (!f)
            {
                ++failed;
                ctx.warn("sounds: could not write " + file);
                continue;
            }
            ++written;
        }
        j.key(std::to_string(id)).beginObject();
        j.field("file", file).field("name", latin1ToUtf8(sound->name));
        j.field("delay", static_cast<std::uint64_t>(sound->replayDelayMs));
        j.endObject();
    }
    j.endObject().endObject();
    j.save((fs::path(dir) / "sounds.json").string());

    m.field("wav", static_cast<std::uint64_t>(written)).field("overridesCopied", static_cast<std::uint64_t>(copied));
    if (failed)
        m.field("failed", static_cast<std::uint64_t>(failed));
    return true;
}

}  // namespace uoconvert
