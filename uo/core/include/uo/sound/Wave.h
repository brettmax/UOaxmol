// SPDX-License-Identifier: BSD-2-Clause
// Part of the AxmolUO client core. Ported from ClassicUO (SoundsLoader, PacketHandlers).
//
// RIFF/WAVE helpers for UO sound data.
//
// Entries in sound.mul / soundLegacyMUL.uop are headerless PCM, always 22,050 Hz, mono, 16-bit
// little-endian. Axmol's AudioEngine only plays files it can decode, so the client wraps that PCM
// in a 44-byte WAV header before handing it over.

#pragma once

#include <cstdint>
#include <span>
#include <vector>

namespace uo::sound
{

inline constexpr uint32_t kUoSampleRate    = 22050;
inline constexpr uint16_t kUoChannels      = 1;
inline constexpr uint16_t kUoBitsPerSample = 16;

struct WaveFormat
{
    uint32_t sampleRate    = 0;
    uint16_t channels      = 0;
    uint16_t bitsPerSample = 0;
};

// A canonical 44-byte header followed by `pcm`.
std::vector<uint8_t> encodeWave(std::span<const uint8_t> pcm,
                                const WaveFormat& format = {kUoSampleRate, kUoChannels, kUoBitsPerSample});

// Walks the RIFF chunks (LIST and fact chunks may come before data) and returns the data chunk.
// Returns false for anything that is not a WAV or has no data chunk.
bool parseWave(std::span<const uint8_t> file, WaveFormat& format, std::span<const uint8_t>& data);

// ClassicUO's replay throttle for UO-format PCM: (bytes - 32) / 88.2 ms. PCM runs at 44.1 bytes/ms,
// so this is about half the sound's length; kept as is for identical feel.
uint32_t uoSoundDelayMs(size_t pcmBytes);

}  // namespace uo::sound
