// SPDX-License-Identifier: BSD-2-Clause
// Ported from ClassicUO (MessageManager.CreateMessage and CalculateTimeToLive).

#include "uo/text/SpeechText.h"

namespace uo::text
{

uint16_t fixSpeechHue(uint16_t hue)
{
    uint16_t fixed = hue & 0x3FFF;

    if (fixed != 0)
    {
        if (fixed >= 0x0BB8)
            fixed = 1;

        fixed |= hue & 0xC000;
    }
    else
    {
        fixed = hue & 0x8000;
    }

    return fixed;
}

int speechLayoutWidth(const FontRenderer& fonts, uint8_t font, bool unicode, std::u16string_view text)
{
    const int width = unicode ? fonts.widthUnicode(font, text) : fonts.widthAscii(font, text);

    if (width <= kSpeechWrapWidth)
        return 0;

    return unicode ? fonts.widthExUnicode(font, text, kSpeechWrapWidth, TextAlign::Left, FontStyleBlackBorder)
                   : fonts.widthExAscii(font, text, kSpeechWrapWidth, TextAlign::Left, FontStyleBlackBorder);
}

int64_t speechTimeToLive(int lineCount, const SpeechDelaySettings& settings)
{
    if (settings.scaleSpeechDelay)
    {
        const int delay = settings.speechDelay < 10 ? 10 : settings.speechDelay;
        return static_cast<int64_t>(4000.0f * lineCount * delay / 100.0f);
    }

    // The original's fixed-point form of speechDelay * 40.
    const int64_t delay = (5497558140000LL * settings.speechDelay) >> 32 >> 5;
    return (delay >> 31) + delay;
}

}  // namespace uo::text
