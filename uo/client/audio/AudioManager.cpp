// SPDX-License-Identifier: BSD-2-Clause

#include "AudioManager.h"

#include "uo/sound/Wave.h"

#include "axmol/audio/AudioEngine.h"
#include "axmol/base/Logging.h"
#include "axmol/platform/FileUtils.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>

namespace fs = std::filesystem;
using ax::AudioEngine;

namespace uo::audio
{

namespace
{

bool alive(int audioId)
{
    return audioId != AudioEngine::INVALID_AUDIO_ID && AudioEngine::getState(audioId) != AudioEngine::AudioState::ERROR;
}

}  // namespace

uint64_t AudioManager::nowMs()
{
    using namespace std::chrono;
    return static_cast<uint64_t>(duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count());
}

bool AudioManager::initialize(sound::SoundLoader& loader, fs::path cacheDir)
{
    _loader = &loader;
    _cacheDir =
        cacheDir.empty() ? fs::path(ax::FileUtils::getInstance()->getWritablePath()) / "uo_sound_cache" : std::move(cacheDir);

    std::error_code ec;
    fs::remove_all(_cacheDir, ec);
    fs::create_directories(_cacheDir, ec);

    if (ec)
    {
        AXLOGW("audio: cannot create sound cache {}: {}", _cacheDir.string(), ec.message());
        _available = false;
        return false;
    }

    // ClassicUO probes the device with a throwaway voice and disables audio on failure.
    _available = AudioEngine::lazyInit();

    if (!_available)
    {
        AXLOGW("audio: no audio device, sound and music are disabled");
    }

    return _available;
}

void AudioManager::shutdown()
{
    stopSounds();
    stopMusic();
    _sounds.clear();

    if (_available)
    {
        AudioEngine::uncacheAll();
    }

    std::error_code ec;
    fs::remove_all(_cacheDir, ec);
}

void AudioManager::setClientVersion(ClientVersion version)
{
    _loginMusicIndex = sound::loginMusicIndex(version);
}

void AudioManager::setSettings(const AudioSettings& settings)
{
    _settings = settings;
}

void AudioManager::setWindowActive(bool active)
{
    _active = active;

    if (_settings.playInBackground)
    {
        return;
    }

    // SoundEffect.MasterVolume = 0/1 in ClassicUO. AudioEngine has no master volume, so every
    // live instance is set, and update() keeps music in step.
    for (const PlayingSound& s : _playing)
    {
        AudioEngine::setVolume(s.audioId, active ? std::max(s.volume - s.distanceFactor, 0.0f) : 0.0f);
    }

    for (const MusicSlot& m : _music)
    {
        if (alive(m.audioId))
        {
            AudioEngine::setVolume(m.audioId, active ? m.volume : 0.0f);
        }
    }
}

float AudioManager::soundVolume() const
{
    if (!_settings.enableSound || !audible())
    {
        return 0.0f;
    }

    return _settings.soundVolume / kVolumeDelta;
}

float AudioManager::musicVolume(bool login) const
{
    if (login)
    {
        return _settings.loginMusic ? _settings.loginMusicVolume / kVolumeDelta : 0.0f;
    }

    return _settings.enableMusic ? _settings.musicVolume / kVolumeDelta : 0.0f;
}

AudioManager::CachedSound* AudioManager::cached(int id)
{
    if (auto it = _sounds.find(id); it != _sounds.end())
    {
        return &it->second;
    }

    std::optional<sound::SoundEffect> fx = _loader ? _loader->sound(id) : std::nullopt;

    if (!fx)
    {
        return nullptr;
    }

    CachedSound entry;
    entry.delayMs = fx->replayDelayMs;

    if (!fx->file.empty())
    {
        entry.path = fx->file.string();
    }
    else
    {
        fs::path out = _cacheDir / (std::to_string(id) + ".wav");
        std::vector<uint8_t> wav = sound::encodeWave(fx->pcm);
        std::ofstream file(out, std::ios::binary | std::ios::trunc);
        file.write(reinterpret_cast<const char*>(wav.data()), static_cast<std::streamsize>(wav.size()));

        if (!file)
        {
            AXLOGW("audio: cannot write {}", out.string());
            return nullptr;
        }

        entry.path = out.string();
    }

    return &_sounds.emplace(id, std::move(entry)).first->second;
}

bool AudioManager::playEffect(int id, float volume, float distanceFactor)
{
    CachedSound* s = cached(id);

    if (!s)
    {
        return false;
    }

    const uint64_t now = nowMs();

    // UOSound.Delay: the same id is held off for about half its length.
    if (s->nextAllowed > now)
    {
        return false;
    }

    // Playing an id again restarts it rather than stacking a second voice (Sound.Play stops
    // its instance first).
    if (alive(s->audioId))
    {
        AudioEngine::stop(s->audioId);
    }

    volume = std::clamp(volume, 0.0f, 1.0f);
    const float effective = std::max(volume - distanceFactor, 0.0f);

    const int audioId = AudioEngine::play2d(s->path, false, effective);

    if (audioId == AudioEngine::INVALID_AUDIO_ID)
    {
        return false;
    }

    s->audioId     = audioId;
    s->nextAllowed = now + s->delayMs;
    _playing.push_back({audioId, volume, distanceFactor});

    return true;
}

void AudioManager::playSound(int id)
{
    if (!_available)
    {
        return;
    }

    playEffect(id, soundVolume(), 0.0f);
}

void AudioManager::playSoundAt(int id, int x, int y, const Listener& listener)
{
    if (!_available || !listener.inGame)
    {
        return;
    }

    const int distance = std::max(std::abs(x - listener.x), std::abs(y - listener.y));

    float volume         = soundVolume();
    float distanceFactor = 0.0f;

    if (distance >= 1)
    {
        distanceFactor = volume / (listener.viewRange + 1) * distance;
    }

    if (distance > listener.viewRange)
    {
        volume = 0.0f;
    }

    // ClassicUO still starts an inaudible voice here; skipping it saves a mixer slot and, with
    // no voice, the throttle is not armed for a sound nobody heard.
    if (volume - distanceFactor <= 0.0f)
    {
        return;
    }

    playEffect(id, volume, distanceFactor);
}

void AudioManager::playMusic(int id, bool warMode, bool login)
{
    if (!_available || id < 0 || id >= sound::SoundLoader::kMaxMusicId)
    {
        return;
    }

    const float volume = musicVolume(login);

    if (!login && warMode && !_settings.enableCombatMusic)
    {
        return;
    }

    const sound::MusicTrack* track = _loader ? _loader->music(id) : nullptr;

    if (!track)
    {
        if (_music[0].track != -1)
        {
            stopMusic();
        }

        return;
    }

    if (id == _music[0].track && !warMode)
    {
        return;
    }

    stopMusic();

    const int slot     = warMode ? 1 : 0;
    _musicIndices[slot] = id;
    _music[slot].track  = id;
    _music[slot].volume = volume;

    if (track->file.empty())
    {
        AXLOGD("audio: music {} ({}) has no playable file", id, track->name);
        return;
    }

    _music[slot].audioId = AudioEngine::play2d(track->file.string(), track->loop, audible() ? volume : 0.0f);
}

void AudioManager::stopMusic()
{
    for (MusicSlot& m : _music)
    {
        if (alive(m.audioId))
        {
            AudioEngine::stop(m.audioId);
        }

        m = {};
    }
}

void AudioManager::stopWarMusic()
{
    playMusic(_musicIndices[0]);
}

void AudioManager::stopSounds()
{
    for (const PlayingSound& s : _playing)
    {
        AudioEngine::stop(s.audioId);
    }

    _playing.clear();
}

void AudioManager::updateCurrentMusicVolume(bool login)
{
    if (!_available)
    {
        return;
    }

    const float volume = musicVolume(login);
    const bool war     = _music[1].track != -1;

    for (size_t i = 0; i < _music.size(); ++i)
    {
        MusicSlot& m = _music[i];

        if (m.track == -1)
        {
            continue;
        }

        m.volume = i == 0 && war ? 0.0f : volume;

        if (alive(m.audioId))
        {
            AudioEngine::setVolume(m.audioId, audible() ? m.volume : 0.0f);
        }
    }
}

void AudioManager::updateCurrentSoundsVolume()
{
    if (!_available)
    {
        return;
    }

    const float volume = soundVolume();

    for (PlayingSound& s : _playing)
    {
        s.volume = volume;
        AudioEngine::setVolume(s.audioId, std::max(volume - s.distanceFactor, 0.0f));
    }
}

void AudioManager::update()
{
    if (!_available)
    {
        return;
    }

    std::erase_if(_playing, [](const PlayingSound& s) { return !alive(s.audioId); });

    // Music follows the profile every frame, as ClassicUO's Update does while focused.
    if (!_settings.playInBackground)
    {
        const bool war = _music[1].track != -1;

        for (size_t i = 0; i < _music.size(); ++i)
        {
            MusicSlot& m = _music[i];

            if (!alive(m.audioId))
            {
                continue;
            }

            float target = 0.0f;

            if (_active)
            {
                target = (i == 0 && war) || !_settings.enableMusic ? 0.0f : _settings.musicVolume / kVolumeDelta;
                m.volume = target;
            }

            if (AudioEngine::getVolume(m.audioId) != target)
            {
                AudioEngine::setVolume(m.audioId, target);
            }
        }
    }
}

void AudioManager::reloadSounds()
{
    stopSounds();

    if (_loader)
    {
        _loader->loadOverrides();
    }

    for (const auto& [id, s] : _sounds)
    {
        AudioEngine::uncache(s.path);
    }

    _sounds.clear();
}

void AudioManager::onPlaySoundPacket(std::span<const uint8_t> payload, const Listener& listener)
{
    if (auto p = sound::parsePlaySound(payload))
    {
        playSoundAt(p->sound, p->x, p->y, listener);
    }
}

void AudioManager::onPlayMusicPacket(std::span<const uint8_t> payload)
{
    auto p = sound::parsePlayMusic(payload);

    if (!p)
    {
        return;
    }

    if (p->stop)
    {
        stopMusic();
    }
    else
    {
        playMusic(p->music);
    }
}

int AudioManager::currentMusic() const
{
    for (const MusicSlot& m : _music)
    {
        if (alive(m.audioId))
        {
            return m.track;
        }
    }

    return -1;
}

}  // namespace uo::audio
