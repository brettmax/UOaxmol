// SPDX-License-Identifier: BSD-2-Clause
// Part of the AxmolUO client. Ported from ClassicUO.Game.UI.Gumps.StatusGump (StatusGumpBase and
// StatusGumpOld, the classic paper status window 0x0802) and HealthBarGump (the classic player
// status bar 0x0803/0x0807 it minimizes to).
//
// The classic client toggles between the two: a click in the bottom-right corner of the status
// window swaps it for the bar at the same place, and a double click on the bar swaps back.
#pragma once

#include "gumps/Controls.h"

#include <array>
#include <cstdint>
#include <string>

namespace uo::world
{
class World;
}

namespace uo::client::gumps
{

class StatusClickPic;
class StatusBarFill;

// StatusGumpOld: name, str/dex/int with stat lock arrows, sex, armor, hits, mana, stamina, gold
// and weight. Values refresh when they change.
class StatusGump final : public Gump
{
public:
    StatusGump(GumpContext& ctx, uo::world::World& world);

    void refresh() override;

    // Replaces this window with the status bar at the same screen position.
    void minimize();

private:
    enum Stat : int
    {
        Name,
        Strength,
        Dexterity,
        Intelligence,
        Sex,
        Armor,
        Hits,
        Mana,
        Stamina,
        Gold,
        Weight,
        Count,
    };

    void setValue(Stat stat, std::string text);
    void cycleStatLock(int stat);
    void showStatLocks();

    uo::world::World& _world;
    std::array<TextLabel*, Count> _labels{};
    std::array<std::string, Count> _values;
    std::array<StatusClickPic*, 3> _lockers{};
    std::array<uint8_t, 3> _shownLocks{0xFF, 0xFF, 0xFF};
};

// HealthBarGump for the player (not in a party): hits, mana and stamina bars, blue normally,
// green when poisoned, yellow for yellow hits; the background turns to the war-mode art.
class StatusBarGump final : public Gump
{
public:
    StatusBarGump(GumpContext& ctx, uo::world::World& world);

    void refresh() override;

    // Replaces the bar with the full status window at the same screen position.
    void maximize();

private:
    enum class HitsLine : uint8_t
    {
        Normal,
        Poisoned,
        Yellow,
    };

    uo::world::World& _world;
    StatusClickPic* _background = nullptr;
    GumpPic* _hitsBack = nullptr;
    std::array<StatusBarFill*, 3> _bars{};
    std::array<int, 3> _shownWidths{-1, -1, -1};
    HitsLine _hitsLine = HitsLine::Normal;
    bool _warMode = false;
};

}  // namespace uo::client::gumps
