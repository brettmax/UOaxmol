// SPDX-License-Identifier: BSD-2-Clause
// Part of the AxmolUO client. A gump sent by the server (0xB0 / 0xDD), built from the parsed
// layout; replies with 0xB1. Ported from ClassicUO's PacketHandlers.CreateGump and Gump.cs.
#pragma once

#include "gumps/Controls.h"

#include "uo/gumps/GumpLayout.h"

namespace uo::client::gumps
{

class ServerGump : public Gump
{
public:
    static ServerGump* create(GumpContext& ctx, const uo::gumps::GumpLayout& layout);

    // Rebuilds in place when the server re-sends the same gump (same sender and type).
    void rebuild(const uo::gumps::GumpLayout& layout);

    uint32_t masterGump() const { return _masterGump; }

    // Replies with the button, checked switches and text entries, then closes.
    void onButton(uint32_t buttonId) override;
    // Right-click close sends button 0, as the classic client does.
    void onCloseRequested() override;
    bool onWheel(Control* target, float dy) override;

    // Builds the 0xB1 reply without sending it (for tests and the manager).
    std::vector<uint8_t> buildReply(uint32_t buttonId) const;

private:
    ServerGump(GumpContext& ctx, uint32_t sender, uint32_t gumpId);

    Control* makeControl(const uo::gumps::GumpElement& e);
    void applyCheckerTrans(const uo::gumps::GumpElement& e);
    std::string virtueTooltip(uint16_t graphic, uint16_t hue) const;

    uint32_t _masterGump = 0;
    bool _textFocused = false;
};

}  // namespace uo::client::gumps
