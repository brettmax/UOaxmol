// SPDX-License-Identifier: BSD-2-Clause
#include "OggVorbis.h"

#include <vorbis/vorbisenc.h>

#include <cstdio>

namespace uoconvert
{

struct OggVorbisWriter::State
{
    std::FILE* file = nullptr;
    int channels    = 0;
    bool ok         = true;
    ogg_stream_state os{};
    vorbis_info vi{};
    vorbis_comment vc{};
    vorbis_dsp_state vd{};
    vorbis_block vb{};

    void put(const ogg_page& page)
    {
        ok &= std::fwrite(page.header, 1, static_cast<std::size_t>(page.header_len), file) ==
              static_cast<std::size_t>(page.header_len);
        ok &= std::fwrite(page.body, 1, static_cast<std::size_t>(page.body_len), file) ==
              static_cast<std::size_t>(page.body_len);
    }

    // Moves every finished block through the encoder and out as pages.
    void drain()
    {
        ogg_packet op;
        ogg_page og;
        while (vorbis_analysis_blockout(&vd, &vb) == 1)
        {
            vorbis_analysis(&vb, nullptr);
            vorbis_bitrate_addblock(&vb);
            while (vorbis_bitrate_flushpacket(&vd, &op))
            {
                ogg_stream_packetin(&os, &op);
                while (ogg_stream_pageout(&os, &og))
                    put(og);
            }
        }
    }
};

OggVorbisWriter::OggVorbisWriter() = default;

OggVorbisWriter::~OggVorbisWriter()
{
    close();
}

bool OggVorbisWriter::open(const std::string& path, int channels, int sampleRate, float quality)
{
    close();
    auto s = std::make_unique<State>();
    s->file = std::fopen(path.c_str(), "wb");
    if (!s->file)
        return false;
    s->channels = channels;
    vorbis_info_init(&s->vi);
    if (vorbis_encode_init_vbr(&s->vi, channels, sampleRate, quality) != 0)
    {
        vorbis_info_clear(&s->vi);
        std::fclose(s->file);
        return false;
    }
    vorbis_comment_init(&s->vc);
    vorbis_comment_add_tag(&s->vc, "ENCODER", "uoconvert");
    vorbis_analysis_init(&s->vd, &s->vi);
    vorbis_block_init(&s->vd, &s->vb);
    ogg_stream_init(&s->os, 0x554F4D55);  // any serial number; "UOMU"

    ogg_packet header, comments, codebooks;
    vorbis_analysis_headerout(&s->vd, &s->vc, &header, &comments, &codebooks);
    ogg_stream_packetin(&s->os, &header);
    ogg_stream_packetin(&s->os, &comments);
    ogg_stream_packetin(&s->os, &codebooks);
    ogg_page og;
    while (ogg_stream_flush(&s->os, &og))
        s->put(og);
    _s = std::move(s);
    return _s->ok;
}

bool OggVorbisWriter::write(const float* interleaved, int frames)
{
    if (!_s || frames <= 0)
        return _s && _s->ok;
    float** buffer = vorbis_analysis_buffer(&_s->vd, frames);
    for (int i = 0; i < frames; ++i)
        for (int c = 0; c < _s->channels; ++c)
            buffer[c][i] = interleaved[i * _s->channels + c];
    vorbis_analysis_wrote(&_s->vd, frames);
    _s->drain();
    return _s->ok;
}

bool OggVorbisWriter::close()
{
    if (!_s)
        return true;
    vorbis_analysis_wrote(&_s->vd, 0);  // end of stream
    _s->drain();
    ogg_page og;
    while (ogg_stream_flush(&_s->os, &og))
        _s->put(og);
    ogg_stream_clear(&_s->os);
    vorbis_block_clear(&_s->vb);
    vorbis_dsp_clear(&_s->vd);
    vorbis_comment_clear(&_s->vc);
    vorbis_info_clear(&_s->vi);
    bool ok = _s->ok && std::fclose(_s->file) == 0;
    _s.reset();
    return ok;
}

}  // namespace uoconvert
