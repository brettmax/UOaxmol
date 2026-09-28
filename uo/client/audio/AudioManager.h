// SPDX-License-Identifier: BSD-2-Clause
//
// Sound effects and music on Axmol's AudioEngine. Port of ClassicUO's AudioManager,
// Renderer/Sounds/Sound, UOSound and UOMusic.
//
// ClassicUO streams raw PCM into FNA voices. AudioEngine only plays files, so archive sounds are
// wrapped in a WAV header and written once per id to a cache folder the manager owns; after that
// AudioEngine's own decoded-buffer cache serves repeats. Music files are played in place.
//
// Main thread only, like the rest of the game loop.

#pragma once

#include "uo/sound/SoundLoader.h"
#include "uo/sound/SoundPackets.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

namespace uo::audio
{

// Mirrors the Profile / Settings fields ClassicUO reads. Volumes are 0..250, as in ClassicUO,
// where playback volume is value / 250.
struct AudioSettings
{
    bool enableSound       = true;
    int soundVolume        = 100;
    bool enableMusic       = true;
    int musicVolume        = 100;
    bool enableCombatMusic = true;
    bool playInBackground  = false;  // ReproduceSoundsInBackground
    bool loginMusic        = true;
    int loginMusicVolume   = 70;
};

// Where the player stands, for 0x54's distance attenuation.
struct Listener
{
    bool inGame   = false;
    int x         = 0;
    int y         = 0;
    int viewRange = 18;  // World.ClientViewRange
};

class AudioManager
{
public:
    static constexpr float kVolumeDelta = 250.0f;  // SOUND_DELTA

    // `cacheDir` empty means <writable path>/uo_sound_cache. The folder is emptied here: it is
    // derived from the loaded archive and must not outlive a change of client files.
    bool initialize(sound::SoundLoader& loader, std::filesystem::path cacheDir = {});
    void shutdown();

    bool available() const { return _available; }

    void setSettings(const AudioSettings& settings);
    const AudioSettings& settings() const { return _settings; }

    // Window focus. Without playInBackground everything goes silent while unfocused.
    void setWindowActive(bool active);

    void playSound(int id);
    void playSoundAt(int id, int x, int y, const Listener& listener);

    void playMusic(int id, bool warMode = false, bool login = false);
    void stopMusic();
    void stopWarMusic();
    void stopSounds();

    void updateCurrentMusicVolume(bool login = false);
    void updateCurrentSoundsVolume();

    // Call once per frame: prunes finished sounds and applies focus muting to music.
    void update();

    // Re-reads the shard's Sounds/ folder and drops cached effects (ClassicUO's Sound.Reload).
    void reloadSounds();

    // Packet entry points; `payload` is everything after the packet id.
    void onPlaySoundPacket(std::span<const uint8_t> payload, const Listener& listener);
    void onPlayMusicPacket(std::span<const uint8_t> payload);

    int loginMusicIndex() const { return _loginMusicIndex; }
    void setClientVersion(ClientVersion version);

    int currentMusic() const;  // playing track id, or -1

private:
    struct CachedSound
    {
        std::string path;
        uint32_t delayMs      = 0;
        uint64_t nextAllowed  = 0;  // ms; ClassicUO's _lastPlayedTime
        int audioId           = -1;  // last instance, restarted rather than stacked
    };

    struct PlayingSound
    {
        int audioId;
        float volume;
        float distanceFactor;
    };

    struct MusicSlot
    {
        int track   = -1;
        int audioId = -1;
        float volume = 0;
    };

    CachedSound* cached(int id);
    bool playEffect(int id, float volume, float distanceFactor);
    float soundVolume() const;
    float musicVolume(bool login) const;
    bool audible() const { return _active || _settings.playInBackground; }
    static uint64_t nowMs();

    sound::SoundLoader* _loader = nullptr;
    std::filesystem::path _cacheDir;
    bool _available = false;
    bool _active    = true;
    AudioSettings _settings;
    int _loginMusicIndex = 0;

    std::unordered_map<int, CachedSound> _sounds;
    std::vector<PlayingSound> _playing;

    // [0] regular, [1] war/death music; ClassicUO's _currentMusic / _currentMusicIndices.
    std::array<MusicSlot, 2> _music;
    std::array<int, 2> _musicIndices{0, 0};
};

}  // namespace uo::audio
