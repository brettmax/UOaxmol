// SPDX-License-Identifier: BSD-2-Clause
// Part of the AxmolUO client. Ported from ClassicUO.Game.UI.Gumps.StandardSkillsGump, with the
// pieces it is built from: Controls.ExpandableScroll, Controls.ScrollArea + ScrollFlag, and
// Managers.SkillsGroupManager's default groups (MakeDefault*).
#include "gumps/client/SkillsGump.h"

#include "gumps/GumpActions.h"

#include "uo/world/World.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <numeric>
#include <span>
#include <vector>

namespace uo::client::gumps
{

namespace
{

constexpr float kDiffY = 22;
constexpr float kScrollMin = 274;  // ExpandableScroll c_ExpandableScrollHeight_Min
constexpr float kScrollMax = 800;  // c_ExpandableScrollHeight_Max
constexpr float kScrollStep = 200; // what one expander click adds (see setScrollHeight)

constexpr uint16_t kPicNormal = 0x082D;
constexpr uint16_t kPicMinimized = 0x0839;
constexpr uint16_t kScrollGraphic = 0x1F40;
constexpr uint16_t kScrollTitle = 0x0834;
constexpr uint16_t kLine = 0x082B;
constexpr uint16_t kComment = 0x0836;
constexpr uint16_t kNewGroup = 0x083A;
constexpr uint16_t kCheckOff = 0x0938;
constexpr uint16_t kCheckOn = 0x0939;
constexpr uint16_t kExpanderNormal = 0x082E;
constexpr uint16_t kExpanderPressed = 0x082F;

constexpr uint16_t kGroupCollapsed = 0x0827;
constexpr uint16_t kGroupExpanded = 0x0826;
constexpr uint16_t kGroupLine = 0x0835;
constexpr uint16_t kUseNormal = 0x0837;
constexpr uint16_t kUsePressed = 0x0838;
constexpr uint16_t kLockUp = 0x0984;
constexpr uint16_t kLockDown = 0x0986;
constexpr uint16_t kLockLocked = 0x082C;
constexpr uint16_t kScrollFlag = 0x0828;

constexpr float kRowHeight = 17;
constexpr float kGroupX = 3;   // SkillsGroupControl(x: 3)
constexpr float kUseX = 8;
constexpr float kNameX = 22;
constexpr float kValueRight = 250;
constexpr float kLockX = 251;

ax::Size gumpSize(GumpContext& ctx, uint16_t id)
{
    auto* tex = ctx.textures->gump(id);
    return tex ? tex->getContentSize() : ax::Size(0, 0);
}

ax::Sprite* gumpSprite(GumpContext& ctx, uint16_t id)
{
    auto* tex = ctx.textures->gump(id);

    if (!tex)
    {
        return nullptr;
    }

    auto* s = ax::Sprite::createWithTexture(tex);
    s->setAnchorPoint(ax::Vec2(0, 1));
    return s;
}

// Adds `child` to `parent` at UO (x, y) against a parent of height `parentHeight`.
void addAt(ax::Node* parent, ax::Node* child, float x, float y, float parentHeight)
{
    if (!child)
    {
        return;
    }

    placeTopLeft(child, x, y, parentHeight);
    parent->addChild(child);
}

uint16_t lockGraphic(uo::world::SkillLock lock)
{
    switch (lock)
    {
    case uo::world::SkillLock::Down:
        return kLockDown;
    case uo::world::SkillLock::Locked:
        return kLockLocked;
    case uo::world::SkillLock::Up:
    default:
        return kLockUp;
    }
}

// Skill.Value / Base / Cap formatted as ToString("F1") from the fixed-point value.
std::string fixed1(int64_t v)
{
    char buf[32];
    const bool negative = v < 0;
    const int64_t a = negative ? -v : v;
    std::snprintf(buf, sizeof(buf), "%s%lld.%lld", negative ? "-" : "", static_cast<long long>(a / 10),
                  static_cast<long long>(a % 10));
    return buf;
}

// SkillsGroupManager.MakeDefault* when skillgrp.mul is absent. Indices past the skill count
// are dropped, as the "count > n" checks do there.
struct DefaultGroup
{
    const char* name;
    std::span<const uint8_t> skills;
};

constexpr uint8_t kMisc[] = {4, 6, 10, 12, 19, 3, 36};
constexpr uint8_t kCombat[] = {1, 31, 42, 17, 41, 5, 40, 27, 57, 43, 50, 51, 52, 53};
constexpr uint8_t kTrade[] = {0, 7, 8, 11, 13, 23, 44, 45, 34, 37};
constexpr uint8_t kMagic[] = {16, 56, 25, 46, 55, 26, 54, 32, 49};
constexpr uint8_t kWilderness[] = {2, 35, 18, 20, 38, 39};
constexpr uint8_t kThieving[] = {14, 21, 24, 30, 48, 28, 33, 47};
constexpr uint8_t kBard[] = {15, 29, 9, 22};

constexpr DefaultGroup kDefaultGroups[] = {
    {"Miscellaneous", kMisc}, {"Combat", kCombat},     {"Trade Skills", kTrade}, {"Magic", kMagic},
    {"Wilderness", kWilderness}, {"Thieving", kThieving}, {"Bard", kBard},
};

bool lessNoCase(const std::string& a, const std::string& b)
{
    return std::lexicographical_compare(a.begin(), a.end(), b.begin(), b.end(), [](unsigned char x, unsigned char y) {
        return std::tolower(x) < std::tolower(y);
    });
}

bool isDescendant(const ax::Node* node, const ax::Node* ancestor)
{
    for (; node; node = node->getParent())
    {
        if (node == ancestor)
        {
            return true;
        }
    }

    return false;
}

}  // namespace

// --- local controls --------------------------------------------------------------------

// A gump picture that reports double clicks (the minimized skills pic).
class SkillsClickPic final : public GumpPic
{
public:
    SkillsClickPic(GumpContext& ctx, uint16_t graphic) : GumpPic(ctx, graphic) {}

    std::function<void()> onDoubleClicked;

    void onDoubleClick(MouseButton button) override
    {
        if (button == MouseButton::Left && onDoubleClicked)
        {
            onDoubleClicked();
        }
    }
};

// HitBox: an invisible clickable rectangle.
class SkillsHitBox final : public Control
{
public:
    SkillsHitBox(float w, float h)
    {
        init();
        autorelease();
        setAcceptsInput(true);
        setUOSize(w, h);
    }

    std::function<void()> onClicked;

    void onClick(MouseButton button) override
    {
        if (button == MouseButton::Left && onClicked)
        {
            onClicked();
        }
    }
};

// ExpandableScroll: top (graphic), two tiled middles (graphic + 1, + 2), bottom (graphic + 3),
// a centred title and the expander button.
class SkillsExpandableScroll final : public Control
{
public:
    SkillsExpandableScroll(GumpContext& ctx, uint16_t graphic, uint16_t title, float height)
        : _ctx(ctx), _graphic(graphic), _title(title)
    {
        init();
        autorelease();
        setAcceptsInput(true);

        for (int i = 0; i < 4; ++i)
        {
            _w[i] = gumpSize(ctx, static_cast<uint16_t>(graphic + i)).width;
            _width = std::max(_width, _w[i]);
        }

        setHeight(height);
    }

    // ClassicUO's Width is the middle piece's; the gump lays the list out from it.
    float middleWidth() const { return _w[2]; }

    void setHeight(float height)
    {
        _height = height;
        build();
    }

    std::function<void()> onExpander;

private:
    void build()
    {
        // The expander stays (the manager may hold it as the hovered or clicked control, and
        // this runs from its own click); only the art is rebuilt.
        for (ax::Node* n : _pieces)
        {
            n->removeFromParent();
        }

        _pieces.clear();
        setUOSize(_width, _height);

        const ax::Size top = gumpSize(_ctx, _graphic);
        const ax::Size bottom = gumpSize(_ctx, static_cast<uint16_t>(_graphic + 3));
        const ax::Size expander = gumpSize(_ctx, kExpanderNormal);
        const float midY = top.height;
        const float midH = _height - top.height - bottom.height - expander.height;
        const float midX = std::floor((_width - _w[1]) / 2);
        const int off = static_cast<int>(_w[0]) - static_cast<int>(_w[3]);

        auto piece = [this](ax::Node* n, float x, float y) {
            if (n)
            {
                addAt(this, n, x, y, _height);
                _pieces.push_back(n);
            }
        };

        piece(gumpSprite(_ctx, _graphic), 0, 0);

        for (int i = 1; i <= 2; ++i)
        {
            if (midH > 0)
            {
                auto* tex = _ctx.textures->gump(static_cast<uint16_t>(_graphic + i));
                piece(TiledTexture::create(tex, _w[i], midH), midX, midY);
            }
        }

        piece(gumpSprite(_ctx, static_cast<uint16_t>(_graphic + 3)), static_cast<float>(off / 2 + off / 4),
              _height - bottom.height - expander.height);

        const ax::Size title = gumpSize(_ctx, _title);
        piece(gumpSprite(_ctx, _title), std::floor((top.width - title.width) / 2),
              std::floor((top.height - title.height) / 2));

        if (!_expander)
        {
            _expander = new GumpButton(_ctx, kExpanderNormal, kExpanderPressed, 0);
            addChild(_expander, 1);
            _expander->onClicked = [this](GumpButton*) {
                if (onExpander)
                {
                    onExpander();
                }
            };
        }

        _expander->setUOPosition(std::floor((_width - expander.width) / 2), _height - expander.height - 2);
    }

    GumpContext& _ctx;
    uint16_t _graphic;
    uint16_t _title;
    std::array<float, 4> _w{};
    float _width = 0;
    float _height = 0;
    std::vector<ax::Node*> _pieces;
    GumpButton* _expander = nullptr;
};

// ScrollArea + DataBox of SkillsGroupControl / SkillItemControl, clipped to the view. The rows
// live under a clipping node (not Controls), so the area takes every click inside it and works
// out the row and the part hit from the press position.
class SkillsListArea final : public Control
{
public:
    SkillsListArea(GumpContext& ctx, uo::world::World& world, float width, float height)
        : _ctx(ctx), _world(world), _width(width)
    {
        init();
        autorelease();
        setAcceptsInput(true);

        _useSize = gumpSize(ctx, kUseNormal);
        _lockSize = gumpSize(ctx, kLockUp);
        _toggleSize = gumpSize(ctx, kGroupCollapsed);
        _flagSize = gumpSize(ctx, kScrollFlag);
        // SkillsGroupControl's height from its children when collapsed: the name box (y -3,
        // height 17), the expand button and the tiled line at y 5.
        _headerHeight = std::max({14.0f, _toggleSize.height, 5 + gumpSize(ctx, kGroupLine).height});

        _clip = ax::ClippingRectangleNode::create();
        addChild(_clip);
        _content = ax::Node::create();
        _clip->addChild(_content);

        // ScrollFlag at X = width - 19, and the area grows by 15 for it.
        _flag = gumpSprite(ctx, kScrollFlag);

        if (_flag)
        {
            addChild(_flag, 1);
        }

        setViewHeight(height);
    }

    void setViewHeight(float height)
    {
        _viewHeight = std::max(0.0f, height);
        setUOSize(_width + 15, _viewHeight);
        _clip->setPosition(ax::Vec2(0, 0));
        _clip->setClippingRegion(ax::Rect(0, 0, _width + 15, _viewHeight));
        scrollTo(_scroll);
    }

    // SkillsGump.LoadSkills over the default groups: every skill the player has, grouped and
    // sorted by name inside each group (SkillsGroup.Sort).
    void rebuild(bool showReal, bool showCaps)
    {
        std::vector<bool> expanded;

        for (const Group& g : _groups)
        {
            expanded.push_back(g.expanded);
        }

        _content->removeAllChildren();
        _groups.clear();
        _pressedUse = nullptr;

        const uo::world::Player* p = _world.player();
        const int count = p ? static_cast<int>(p->skills.size()) : 0;
        _skillCount = count;

        for (const DefaultGroup& def : kDefaultGroups)
        {
            Group g;
            g.name = def.name;

            for (uint8_t index : def.skills)
            {
                if (index < count)
                {
                    g.skills.push_back(index);
                }
            }

            std::stable_sort(g.skills.begin(), g.skills.end(),
                             [this](int a, int b) { return lessNoCase(skillName(a), skillName(b)); });
            g.expanded = _groups.size() < expanded.size() && expanded[_groups.size()];
            _groups.push_back(std::move(g));
        }

        for (Group& g : _groups)
        {
            buildGroup(g);
        }

        arrange();
        refreshValues(showReal, showCaps, true);
    }

    int skillCount() const { return _skillCount; }

    // Refreshes values and locks that changed (SkillItemControl.UpdateValueText / SetStatus).
    void refreshValues(bool showReal, bool showCaps, bool force)
    {
        const uo::world::Player* p = _world.player();

        if (!p)
        {
            return;
        }

        for (Group& g : _groups)
        {
            for (Row& r : g.rows)
            {
                if (r.index >= static_cast<int>(p->skills.size()))
                {
                    continue;
                }

                const uo::world::Skill& s = p->skills[r.index];
                const int v = showReal ? s.baseFixed : showCaps ? s.capFixed : s.valueFixed;

                if (force || v != r.shownValue)
                {
                    r.shownValue = v;
                    r.value->setText(fixed1(v));
                    placeTopLeft(r.value, kValueRight - r.value->getContentSize().width, 0, 0);
                }

                const auto lock = static_cast<int>(s.lock);

                if (force || lock != r.shownLock)
                {
                    r.shownLock = lock;
                    r.lock->setGraphic(lockGraphic(s.lock));
                }
            }
        }
    }

    float maxScroll() const { return std::max(0.0f, _contentHeight - _viewHeight); }

    void scrollTo(float value)
    {
        _scroll = std::clamp(value, 0.0f, maxScroll());
        _content->setPosition(ax::Vec2(0, _viewHeight + _scroll));
        updateFlag();
    }

    void scrollBy(float dy) { scrollTo(_scroll + dy); }

    void onMouseDown(MouseButton button, const ax::Vec2& local) override
    {
        _press = local;

        if (button != MouseButton::Left)
        {
            return;
        }

        // Clicking the flag's track jumps there (ScrollFlag.CalculateByPosition).
        if (onFlagTrack(local))
        {
            const float track = _viewHeight - _flagSize.height;

            if (track > 0)
            {
                scrollTo((local.y - _flagSize.height / 2) / track * maxScroll());
            }

            return;
        }

        Hit hit = hitAt(local);

        if (hit.part == Part::Use)
        {
            _pressedUse = hit.row;
            _pressedUse->use->setGraphic(kUsePressed);
        }
    }

    void onMouseUp(MouseButton, bool) override
    {
        if (_pressedUse)
        {
            _pressedUse->use->setGraphic(kUseNormal);
            _pressedUse = nullptr;
        }
    }

    void onClick(MouseButton button) override
    {
        if (button != MouseButton::Left || onFlagTrack(_press))
        {
            return;
        }

        Hit hit = hitAt(_press);

        switch (hit.part)
        {
        case Part::Toggle:
            hit.group->expanded = !hit.group->expanded;
            hit.group->toggle->setGraphic(hit.group->expanded ? kGroupExpanded : kGroupCollapsed);
            arrange();
            break;

        case Part::Use:
            if (_ctx.actions)
            {
                _ctx.actions->useSkill(hit.row->index);
            }
            break;

        case Part::Lock:
            cycleLock(*hit.row);
            break;

        case Part::None:
            break;
        }
    }

    // Two quick clicks on different rows (or the same lock twice) are two clicks.
    void onDoubleClick(MouseButton button) override { onClick(button); }

private:
    struct Row
    {
        int index = 0;
        ax::Node* node = nullptr;
        GumpPic* use = nullptr;
        GumpPic* lock = nullptr;
        TextLabel* value = nullptr;
        int shownValue = -1;
        int shownLock = -1;
    };

    struct Group
    {
        std::string name;
        std::vector<int> skills;
        bool expanded = false;
        ax::Node* node = nullptr;
        ax::Node* box = nullptr;
        GumpPic* toggle = nullptr;
        std::vector<Row> rows;
        float y = 0;
        float height = 0;
    };

    enum class Part : uint8_t
    {
        None,
        Toggle,
        Use,
        Lock,
    };

    struct Hit
    {
        Part part = Part::None;
        Group* group = nullptr;
        Row* row = nullptr;
    };

    std::string skillName(int index) const
    {
        const uo::world::Player* p = _world.player();

        if (p && index < static_cast<int>(p->skills.size()) && !p->skills[index].name.empty())
        {
            return p->skills[index].name;
        }

        if (index < static_cast<int>(_world.skillNames.size()))
        {
            return _world.skillNames[index];
        }

        return "Skill " + std::to_string(index);
    }

    void buildGroup(Group& g)
    {
        g.node = ax::Node::create();
        _content->addChild(g.node);

        // Header: expand button, name (font 6), and the line after the name up to x 215.
        g.toggle = new GumpPic(_ctx, g.expanded ? kGroupExpanded : kGroupCollapsed);
        g.toggle->setVisible(!g.skills.empty());
        addAt(g.node, g.toggle, 0, 0, 0);

        GumpTextStyle nameStyle;
        nameStyle.font = 6;
        nameStyle.unicode = false;
        nameStyle.hue = 0;
        addAt(g.node, new TextLabel(_ctx, g.name, nameStyle), 16, -3, 0);

        const float xx = _ctx.text->measure(g.name, nameStyle).width + 11 + 16;

        if (xx < 215)
        {
            addAt(g.node,
                  TiledTexture::create(_ctx.textures->gump(kGroupLine), 215 - xx, gumpSize(_ctx, kGroupLine).height),
                  xx, 5, 0);
        }

        g.box = ax::Node::create();
        g.node->addChild(g.box);

        GumpTextStyle rowStyle;
        rowStyle.font = 9;
        rowStyle.unicode = false;
        rowStyle.hue = 0x0288;

        const uo::world::Player* p = _world.player();

        for (size_t i = 0; i < g.skills.size(); ++i)
        {
            Row r;
            r.index = g.skills[i];
            r.node = ax::Node::create();
            placeTopLeft(r.node, 0, kRowHeight + static_cast<float>(i) * kRowHeight, 0);
            g.box->addChild(r.node);

            if (p && p->skills[r.index].hasButton)
            {
                r.use = new GumpPic(_ctx, kUseNormal);
                addAt(r.node, r.use, kUseX, 0, 0);
            }

            r.lock = new GumpPic(_ctx, kLockUp);
            addAt(r.node, r.lock, kLockX, 0, 0);
            addAt(r.node, new TextLabel(_ctx, skillName(r.index), rowStyle), kNameX, 0, 0);
            r.value = new TextLabel(_ctx, "", rowStyle);
            addAt(r.node, r.value, kValueRight, 0, 0);
            g.rows.push_back(r);
        }
    }

    // DataBox.ReArrangeChildren: groups stacked by their current height.
    void arrange()
    {
        float y = 0;

        for (Group& g : _groups)
        {
            g.y = y;
            g.height = g.expanded && !g.rows.empty() ? kRowHeight + static_cast<float>(g.rows.size()) * kRowHeight
                                                     : _headerHeight;
            g.box->setVisible(g.expanded);
            placeTopLeft(g.node, kGroupX, y, 0);
            y += g.height;
        }

        _contentHeight = y;
        scrollTo(_scroll);
    }

    bool onFlagTrack(const ax::Vec2& local) const
    {
        const float x = _width - 19;
        return _flag && maxScroll() > 0 && local.x >= x && local.x < x + _flagSize.width;
    }

    void updateFlag()
    {
        if (!_flag)
        {
            return;
        }

        const float max = maxScroll();
        _flag->setVisible(max > 0);

        if (max > 0)
        {
            const float track = std::max(0.0f, _viewHeight - _flagSize.height);
            placeTopLeft(_flag, _width - 19, std::round(track * _scroll / max), _viewHeight);
        }
    }

    static bool inside(const ax::Vec2& p, float x, float y, const ax::Size& size)
    {
        return p.x >= x && p.x < x + size.width && p.y >= y && p.y < y + size.height;
    }

    Hit hitAt(const ax::Vec2& local)
    {
        Hit hit;

        if (local.y < 0 || local.y >= _viewHeight)
        {
            return hit;
        }

        const ax::Vec2 p(local.x - kGroupX, local.y + _scroll);

        for (Group& g : _groups)
        {
            if (p.y < g.y || p.y >= g.y + g.height)
            {
                continue;
            }

            hit.group = &g;
            const float gy = p.y - g.y;

            if (gy < kRowHeight || !g.expanded)
            {
                // The expand button is shown only for groups with skills (ContainsByBounds).
                if (!g.rows.empty() && inside(ax::Vec2(p.x, gy), 0, 0, _toggleSize))
                {
                    hit.part = Part::Toggle;
                }

                return hit;
            }

            const auto i = static_cast<size_t>((gy - kRowHeight) / kRowHeight);

            if (i >= g.rows.size())
            {
                return hit;
            }

            Row& r = g.rows[i];
            const ax::Vec2 rp(p.x, gy - kRowHeight - static_cast<float>(i) * kRowHeight);
            hit.row = &r;

            if (r.use && inside(rp, kUseX, 0, _useSize))
            {
                hit.part = Part::Use;
            }
            else if (inside(rp, kLockX, 0, _lockSize))
            {
                hit.part = Part::Lock;
            }

            return hit;
        }

        return hit;
    }

    // SkillItemControl.OnButtonClick(1): Up -> Down -> Locked -> Up, sent and shown at once.
    void cycleLock(Row& r)
    {
        uo::world::Player* p = _world.player();

        if (!p || r.index >= static_cast<int>(p->skills.size()))
        {
            return;
        }

        uo::world::Skill& s = p->skills[r.index];
        const auto next = static_cast<uint8_t>((static_cast<uint8_t>(s.lock) + 1) % 3);

        if (_ctx.actions)
        {
            _ctx.actions->setSkillLock(static_cast<uint16_t>(r.index), next);
        }

        s.lock = static_cast<uo::world::SkillLock>(next);
        r.shownLock = next;
        r.lock->setGraphic(lockGraphic(s.lock));
    }

    GumpContext& _ctx;
    uo::world::World& _world;
    float _width;
    float _viewHeight = 0;
    float _contentHeight = 0;
    float _scroll = 0;
    float _headerHeight = kRowHeight;
    ax::Size _useSize, _lockSize, _toggleSize, _flagSize;
    ax::ClippingRectangleNode* _clip = nullptr;
    ax::Node* _content = nullptr;
    ax::Sprite* _flag = nullptr;
    std::vector<Group> _groups;
    int _skillCount = -1;
    ax::Vec2 _press;
    Row* _pressedUse = nullptr;
};

// --- SkillsGump ------------------------------------------------------------------------

SkillsGump::SkillsGump(GumpContext& ctx, uo::world::World& world)
    : Gump(ctx, GumpKind::Skills, world.player() ? world.player()->serial : 0, 0), _world(world)
{
    init();
    autorelease();
    setCanMove(true);
    setCanCloseWithRightClick(true);

    _pic = add(new SkillsClickPic(ctx, kPicNormal));
    _pic->setUOPosition(160, 0);
    _pic->onDoubleClicked = [this] {
        if (_minimized)
        {
            setMinimized(false);
        }
    };

    _scroll = add(new SkillsExpandableScroll(ctx, kScrollGraphic, kScrollTitle, _scrollHeight));
    _scroll->setUOPosition(0, kDiffY);
    _scroll->onExpander = [this] {
        // The classic expander is dragged; without pointer-move routing to controls a click
        // steps the height up and wraps back to the minimum.
        setScrollHeight(_scrollHeight >= kScrollMax ? kScrollMin : _scrollHeight + kScrollStep);
    };

    add(new GumpPic(ctx, kLine))->setUOPosition(50, 35 + kDiffY);
    _bottomLine = add(new GumpPic(ctx, kLine));
    _bottomComment = add(new GumpPic(ctx, kComment));

    _areaY = 45 + kDiffY + gumpSize(ctx, kLine).height - 10;
    _area = add(new SkillsListArea(ctx, world, _scroll->middleWidth() - 14, 0));

    GumpTextStyle totalStyle;
    totalStyle.font = 3;
    totalStyle.unicode = false;
    totalStyle.hue = 600;
    _total = add(new TextLabel(ctx, "", totalStyle));

    // New group (0x083A). Groups cannot be renamed or filled without text focus and control
    // drag, so the button is drawn but does nothing (see the report on SkillsGump).
    _newGroup = add(new GumpButton(ctx, kNewGroup, kNewGroup, kNewGroup));

    GumpTextStyle checkStyle;
    checkStyle.font = 1;
    checkStyle.unicode = false;
    checkStyle.hue = 0x0386;

    _checkReal = add(new Checkbox(ctx, kCheckOff, kCheckOn, false));
    _checkRealText = add(new TextLabel(ctx, " - Show Real", checkStyle));
    _checkCaps = add(new Checkbox(ctx, kCheckOff, kCheckOn, false));
    _checkCapsText = add(new TextLabel(ctx, " - Show Caps", checkStyle));
    _checkReal->onChanged = [this](Checkbox* c) { onCheckChanged(c); };
    _checkCaps->onChanged = [this](Checkbox* c) { onCheckChanged(c); };

    _minimizeBox = add(new SkillsHitBox(23, 24));
    _minimizeBox->setUOPosition(160, 0);
    _minimizeBox->onClicked = [this] {
        if (!_minimized)
        {
            setMinimized(true);
        }
    };

    if (world.player() && world.player()->skills.empty() && ctx.actions)
    {
        ctx.actions->requestSkills(world.player()->serial);
    }

    _area->rebuild(false, false);
    layout();
    updateTotal(true);
}

void SkillsGump::layout()
{
    // StandardSkillsGump.Update, with the gump's height = the scroll's bottom.
    const float h = kDiffY + _scrollHeight;
    const float newGroupW = _newGroup->getContentSize().width;
    const float checkX = 60 + newGroupW + 30;
    const float checkW = _checkReal->getContentSize().width;

    _bottomLine->setUOPosition(50, h - 98);
    _bottomComment->setUOPosition(25, h - 85);
    _area->setUOPosition(22, _areaY);
    _area->setViewHeight(h - (150 + kDiffY));
    _total->setUOPosition(25 + _bottomComment->getContentSize().width + 5, h - 85 + 2);
    _newGroup->setUOPosition(60, h - 52);
    _checkReal->setUOPosition(checkX, h - 52 - 6);
    _checkRealText->setUOPosition(checkX + checkW, h - 52 - 6);
    _checkCaps->setUOPosition(checkX, h - 52 + 7);
    _checkCapsText->setUOPosition(checkX + checkW, h - 52 + 7);
}

void SkillsGump::setScrollHeight(float height)
{
    height = std::clamp(height, kScrollMin, kScrollMax);

    if (height == _scrollHeight)
    {
        return;
    }

    _scrollHeight = height;
    _scroll->setHeight(height);
    layout();
}

void SkillsGump::setMinimized(bool minimized)
{
    if (_minimized == minimized)
    {
        return;
    }

    _minimized = minimized;

    for (Control* c : controls())
    {
        c->setVisible(!minimized);
    }

    _pic->setVisible(true);
    _pic->setGraphic(minimized ? kPicMinimized : kPicNormal);
    _pic->setUOPosition(minimized ? 0 : 160, 0);
}

void SkillsGump::onCheckChanged(Checkbox* changed)
{
    // Show real and show caps exclude each other; the one just clicked wins.
    if (_checkReal->isChecked() && _checkCaps->isChecked())
    {
        (changed == _checkReal ? _checkCaps : _checkReal)->setChecked(false);
    }

    _area->refreshValues(_checkReal->isChecked(), _checkCaps->isChecked(), true);
    updateTotal(true);
}

void SkillsGump::updateTotal(bool force)
{
    const uo::world::Player* p = _world.player();

    if (!p)
    {
        return;
    }

    // SumTotalSkills: base when showing real, the value otherwise (also while showing caps).
    const bool real = _checkReal->isChecked();
    const int64_t sum = std::accumulate(p->skills.begin(), p->skills.end(), int64_t{0},
                                        [real](int64_t acc, const uo::world::Skill& s) {
                                            return acc + (real ? s.baseFixed : s.valueFixed);
                                        });

    if (force || sum != _shownTotal || real != _shownTotalReal)
    {
        _shownTotal = sum;
        _shownTotalReal = real;
        _total->setText(fixed1(sum));
    }
}

void SkillsGump::refresh()
{
    const uo::world::Player* p = _world.player();

    if (!p || isClosed())
    {
        return;
    }

    const bool real = _checkReal->isChecked();
    const bool caps = _checkCaps->isChecked();

    // The skill list (0x3A full list) arrived or changed size: rebuild the groups.
    if (static_cast<int>(p->skills.size()) != _area->skillCount())
    {
        _area->rebuild(real, caps);
        updateTotal(true);
        return;
    }

    _area->refreshValues(real, caps, false);
    updateTotal(false);
}

bool SkillsGump::onWheel(Control* target, float dy)
{
    if (_minimized || !target || !isDescendant(target, _area))
    {
        return false;
    }

    _area->scrollBy(dy);
    return true;
}

}  // namespace uo::client::gumps
