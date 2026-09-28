// SPDX-License-Identifier: BSD-2-Clause
// Built-in MIDI rendering: TinySoundFont (vendored in third_party) plays a .mid through a
// SoundFont 2 bank, and OggVorbisWriter encodes the result. No external tools needed.
#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace uoconvert
{

class MidiRenderer
{
public:
    static constexpr int kSampleRate = 44100;

    MidiRenderer();
    ~MidiRenderer();
    MidiRenderer(const MidiRenderer&)            = delete;
    MidiRenderer& operator=(const MidiRenderer&) = delete;

    bool loadSoundFont(const std::vector<std::uint8_t>& sf2);

    // Renders `midi` to 44.1 kHz stereo Ogg Vorbis at `path`. Safe to call from several threads
    // at once: each call plays on its own copy of the synthesizer.
    bool renderToOgg(const std::vector<std::uint8_t>& midi, const std::string& path, float quality,
                     std::string& error) const;

private:
    struct Bank;
    std::unique_ptr<Bank> _bank;
};

}  // namespace uoconvert
