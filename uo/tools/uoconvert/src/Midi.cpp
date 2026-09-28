// SPDX-License-Identifier: BSD-2-Clause
#include "Midi.h"

#include "OggVorbis.h"

#include <mutex>

#define TSF_IMPLEMENTATION
#include "tsf.h"
#define TML_IMPLEMENTATION
#include "tml.h"

namespace uoconvert
{

namespace
{

constexpr int kBlock          = 512;       // frames rendered between MIDI event checks (~11.6 ms)
constexpr double kMaxTailMs   = 10000.0;   // longest release tail kept after the last event
constexpr double kMaxLengthMs = 3600000.0; // guard against a corrupt file that never ends

}  // namespace

struct MidiRenderer::Bank
{
    tsf* font = nullptr;
    // tsf_copy bumps a shared, non-atomic reference count.
    mutable std::mutex copyLock;
    ~Bank()
    {
        if (font)
            tsf_close(font);
    }
};

MidiRenderer::MidiRenderer() = default;
MidiRenderer::~MidiRenderer() = default;

bool MidiRenderer::loadSoundFont(const std::vector<std::uint8_t>& sf2)
{
    auto bank  = std::make_unique<Bank>();
    bank->font = tsf_load_memory(sf2.data(), static_cast<int>(sf2.size()));
    if (!bank->font)
        return false;
    _bank = std::move(bank);
    return true;
}

bool MidiRenderer::renderToOgg(const std::vector<std::uint8_t>& midi, const std::string& path, float quality,
                               std::string& error) const
{
    if (!_bank)
    {
        error = "no soundfont loaded";
        return false;
    }
    tml_message* song = tml_load_memory(midi.data(), static_cast<int>(midi.size()));
    if (!song)
    {
        error = "not a readable MIDI file";
        return false;
    }
    tsf* f = nullptr;
    {
        std::lock_guard g(_bank->copyLock);
        f = tsf_copy(_bank->font);
    }
    if (!f)
    {
        tml_free(song);
        error = "out of memory";
        return false;
    }
    tsf_set_output(f, TSF_STEREO_INTERLEAVED, kSampleRate, 0.0f);
    tsf_set_max_voices(f, 256);
    tsf_channel_set_bank_preset(f, 9, 128, 0);  // General MIDI drums on channel 10

    OggVorbisWriter out;
    bool ok = out.open(path, 2, kSampleRate, quality);
    if (!ok)
        error = "cannot write " + path;

    float buffer[kBlock * 2];
    double ms              = 0.0;
    double tail            = 0.0;
    const tml_message* msg = song;
    while (ok && ms < kMaxLengthMs)
    {
        ms += kBlock * 1000.0 / kSampleRate;
        for (; msg && msg->time <= ms; msg = msg->next)
        {
            switch (msg->type)
            {
                case TML_PROGRAM_CHANGE:
                    tsf_channel_set_presetnumber(f, msg->channel, msg->program, msg->channel == 9);
                    break;
                case TML_NOTE_ON: tsf_channel_note_on(f, msg->channel, msg->key, msg->velocity / 127.0f); break;
                case TML_NOTE_OFF: tsf_channel_note_off(f, msg->channel, msg->key); break;
                case TML_PITCH_BEND: tsf_channel_set_pitchwheel(f, msg->channel, msg->pitch_bend); break;
                case TML_CONTROL_CHANGE: tsf_channel_midi_control(f, msg->channel, msg->control, msg->control_value); break;
                default: break;
            }
        }
        if (!msg)
        {
            // Past the last event: keep rendering until the notes have died away.
            if (tsf_active_voice_count(f) == 0 || tail >= kMaxTailMs)
                break;
            tail += kBlock * 1000.0 / kSampleRate;
        }
        tsf_render_float(f, buffer, kBlock, 0);
        ok = out.write(buffer, kBlock);
        if (!ok)
            error = "write failed for " + path;
    }
    if (!out.close() && ok)
    {
        ok    = false;
        error = "write failed for " + path;
    }
    tsf_close(f);
    tml_free(song);
    return ok;
}

}  // namespace uoconvert
