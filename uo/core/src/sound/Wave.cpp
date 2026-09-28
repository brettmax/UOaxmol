// SPDX-License-Identifier: BSD-2-Clause
// Part of the AxmolUO client core. Ported from ClassicUO.Assets (SoundsLoader, WAV handling).

#include "uo/sound/Wave.h"

#include <cstring>

namespace uo::sound
{

namespace
{

void put16(std::vector<uint8_t>& out, uint16_t v)
{
    out.push_back(static_cast<uint8_t>(v));
    out.push_back(static_cast<uint8_t>(v >> 8));
}

void put32(std::vector<uint8_t>& out, uint32_t v)
{
    put16(out, static_cast<uint16_t>(v));
    put16(out, static_cast<uint16_t>(v >> 16));
}

void putTag(std::vector<uint8_t>& out, const char (&tag)[5])
{
    out.insert(out.end(), tag, tag + 4);
}

uint16_t get16(const uint8_t* p)
{
    return static_cast<uint16_t>(p[0] | (p[1] << 8));
}

uint32_t get32(const uint8_t* p)
{
    return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) | (static_cast<uint32_t>(p[2]) << 16) |
           (static_cast<uint32_t>(p[3]) << 24);
}

}  // namespace

std::vector<uint8_t> encodeWave(std::span<const uint8_t> pcm, const WaveFormat& format)
{
    const uint32_t blockAlign = format.channels * (format.bitsPerSample / 8u);
    const auto dataSize       = static_cast<uint32_t>(pcm.size());

    std::vector<uint8_t> out;
    out.reserve(44 + pcm.size());

    putTag(out, "RIFF");
    put32(out, 36 + dataSize);
    putTag(out, "WAVE");

    putTag(out, "fmt ");
    put32(out, 16);
    put16(out, 1);  // PCM
    put16(out, format.channels);
    put32(out, format.sampleRate);
    put32(out, format.sampleRate * blockAlign);
    put16(out, static_cast<uint16_t>(blockAlign));
    put16(out, format.bitsPerSample);

    putTag(out, "data");
    put32(out, dataSize);
    out.insert(out.end(), pcm.begin(), pcm.end());

    return out;
}

bool parseWave(std::span<const uint8_t> file, WaveFormat& format, std::span<const uint8_t>& data)
{
    const uint8_t* raw = file.data();
    const size_t len   = file.size();

    if (len < 12 || std::memcmp(raw, "RIFF", 4) != 0 || std::memcmp(raw + 8, "WAVE", 4) != 0)
    {
        return false;
    }

    format    = {};
    size_t at = 12;

    while (at + 8 <= len)
    {
        size_t size = get32(raw + at + 4);

        if (size > len - at - 8)
        {
            size = len - at - 8;
        }

        if (std::memcmp(raw + at, "fmt ", 4) == 0 && size >= 16)
        {
            format.channels      = get16(raw + at + 10);
            format.sampleRate    = get32(raw + at + 12);
            format.bitsPerSample = get16(raw + at + 22);
        }
        else if (std::memcmp(raw + at, "data", 4) == 0)
        {
            data = file.subspan(at + 8, size);
            return format.channels != 0;
        }

        at += 8 + size + (size & 1);  // chunks are padded to an even length
    }

    return false;
}

uint32_t uoSoundDelayMs(size_t pcmBytes)
{
    if (pcmBytes <= 32)
    {
        return 0;
    }

    return static_cast<uint32_t>(static_cast<double>(pcmBytes - 32) / 88.2);
}

}  // namespace uo::sound
