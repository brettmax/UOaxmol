// SPDX-License-Identifier: BSD-2-Clause
// Part of the AxmolUO client. Ported from ClassicUO.Game.UI.Gumps.StandardSkillsGump (with
// ExpandableScroll, ScrollArea/ScrollFlag and the default groups of SkillsGroupManager).
//
// The classic skills scroll: skills in collapsible groups, a use button for skills that have
// one, a lock arrow per skill (up / down / locked), the total, and "show real" / "show caps"
// switches. The list scrolls with the wheel or by clicking the scroll flag's track.
#pragma once

#include "gumps/Controls.h"

#include <cstdint>
#include <string>

namespace uo::world
{
class World;
}

namespace uo::client::gumps
{

class SkillsClickPic;
class SkillsHitBox;
class SkillsExpandableScroll;
class SkillsListArea;

class SkillsGump final : public Gump
{
public:
    SkillsGump(GumpContext& ctx, uo::world::World& world);

    void refresh() override;
    bool onWheel(Control* target, float dy) override;

    bool isMinimized() const { return _minimized; }
    void setMinimized(bool minimized);

    // Height of the scroll (ExpandableScroll.SpecialHeight), clamped to 274..800.
    void setScrollHeight(float height);
    float scrollHeight() const { return _scrollHeight; }

private:
    void layout();
    void onCheckChanged(Checkbox* changed);
    void updateTotal(bool force);

    uo::world::World& _world;
    SkillsClickPic* _pic = nullptr;
    SkillsExpandableScroll* _scroll = nullptr;
    GumpPic* _bottomLine = nullptr;
    GumpPic* _bottomComment = nullptr;
    SkillsListArea* _area = nullptr;
    TextLabel* _total = nullptr;
    GumpButton* _newGroup = nullptr;
    Checkbox* _checkReal = nullptr;
    Checkbox* _checkCaps = nullptr;
    TextLabel* _checkRealText = nullptr;
    TextLabel* _checkCapsText = nullptr;
    SkillsHitBox* _minimizeBox = nullptr;

    float _scrollHeight = 274;
    float _areaY = 0;
    bool _minimized = false;
    int64_t _shownTotal = -1;
    bool _shownTotalReal = false;
};

}  // namespace uo::client::gumps
