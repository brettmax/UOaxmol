// SPDX-License-Identifier: BSD-2-Clause
// Streaming Ogg Vorbis encoder over the libvorbis Axmol vendors in 3rdparty/ogg.
#pragma once

#include <memory>
#include <string>

namespace uoconvert
{

class OggVorbisWriter
{
public:
    OggVorbisWriter();
    ~OggVorbisWriter();
    OggVorbisWriter(const OggVorbisWriter&)            = delete;
    OggVorbisWriter& operator=(const OggVorbisWriter&) = delete;

    // `quality` is libvorbis VBR quality, -0.1 .. 1.0; 0.5 matches `oggenc -q 5`.
    bool open(const std::string& path, int channels, int sampleRate, float quality);
    // `frames` frames of interleaved samples in [-1, 1].
    bool write(const float* interleaved, int frames);
    // Flushes the last pages. Returns false if any write failed.
    bool close();

private:
    struct State;
    std::unique_ptr<State> _s;
};

}  // namespace uoconvert
