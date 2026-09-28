// SPDX-License-Identifier: BSD-2-Clause
// Ported from ClassicUO (MessageManager.CreateMessage and CalculateTimeToLive).

#pragma once

#include "uo/text/FontRenderer.h"
#include "uo/text/MessageTypes.h"

#include <cstdint>
#include <string_view>

namespace uo::text
{

// Overhead speech wraps at this width.
inline constexpr int kSpeechWrapWidth = 200;

// Clamps a speech hue the way the client does: hue ids past 0x0BB7 fall back to 1,
// the partial/translucent bits (0xC000) are kept.
uint16_t fixSpeechHue(uint16_t hue);

// Width to lay speech out at: 0 (natural width) when it fits in 200 pixels, else the
// widest line when wrapped at 200.
int speechLayoutWidth(const FontRenderer& fonts, uint8_t font, bool unicode, std::u16string_view text);

struct SpeechDelaySettings
{
    bool scaleSpeechDelay = true;  // Profile.ScaleSpeechDelay
    int speechDelay       = 100;   // Profile.SpeechDelay
};

// How long overhead text stays up, in milliseconds.
int64_t speechTimeToLive(int lineCount, const SpeechDelaySettings& settings);

}  // namespace uo::text
