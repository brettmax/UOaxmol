// SPDX-License-Identifier: BSD-2-Clause
// Part of the AxmolUO client. Ported from ClassicUO.Game.UI.Gumps.StatusGump (StatusGumpOld) and
// HealthBarGump (the player's own classic bar).
#include "gumps/client/StatusGump.h"

#include "gumps/GumpActions.h"
#include "gumps/GumpManager.h"

#include "uo/world/World.h"

#include <algorithm>
#include <cstdio>

namespace uo::client::gumps
{

// --- local controls --------------------------------------------------------------------

// A gump picture that reports clicks and double clicks (ClassicUO hooks MouseUp /
// MouseDoubleClick on plain GumpPics for the stat lockers and the status bar).
class StatusClickPic final : public GumpPic
{
public:
    StatusClickPic(GumpContext& ctx, uint16_t graphic) : GumpPic(ctx, graphic) {}

    std::function<void()> onClicked;
    std::function<void()> onDoubleClicked;

    void onClick(MouseButton button) override
    {
        if (button == MouseButton::Left && onClicked)
        {
            onClicked();
        }
    }

    void onDoubleClick(MouseButton button) override
    {
        if (button != MouseButton::Left)
        {
            return;
        }

        if (onDoubleClicked)
        {
            onDoubleClicked();
        }
        else if (onClicked)
        {
            // Two quick clicks on a locker are two clicks, not a double click.
            onClicked();
        }
    }
};

// GumpPicWithWidth: a bar gump drawn (tiled) to `width` pixels.
class StatusBarFill final : public Control
{
public:
    StatusBarFill(GumpContext& ctx, uint16_t graphic) : _ctx(ctx), _graphic(graphic)
    {
        init();
        autorelease();
        setMovesGump(true);
        auto* tex = _ctx.textures->gump(_graphic);
        _height = tex ? tex->getContentSize().height : 0;
        setUOSize(0, _height);
    }

    void setGraphic(uint16_t graphic)
    {
        if (graphic != _graphic)
        {
            _graphic = graphic;
            rebuild();
        }
    }

    void setWidth(int width)
    {
        if (width != _width)
        {
            _width = width;
            rebuild();
        }
    }

private:
    void rebuild()
    {
        removeAllChildren();
        setUOSize(static_cast<float>(_width), _height);

        if (_width <= 0)
        {
            return;
        }

        auto* tiles = TiledTexture::create(_ctx.textures->gump(_graphic), static_cast<float>(_width), _height);
        placeTopLeft(tiles, 0, 0, _height);
        addChild(tiles);
    }

    GumpContext& _ctx;
    uint16_t _graphic;
    int _width = 0;
    float _height = 0;
};

// HitBox: an invisible rectangle with a tooltip that can also take a click.
class StatusHitBox final : public Control
{
public:
    StatusHitBox(float x, float y, float w, float h, std::string tooltip)
    {
        init();
        autorelease();
        setAcceptsInput(true);
        setUOSize(w, h);
        setUOPosition(x, y);
        setTooltip(std::move(tooltip));
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

namespace
{

constexpr uint16_t kStatusBackground = 0x0802;
constexpr uint16_t kLockUp = 0x0984;
constexpr uint16_t kLockDown = 0x0986;
constexpr uint16_t kLockLocked = 0x082C;
constexpr uint16_t kTextHue = 0x0386;
// StatusGumpOld: a click at or past this point (the corner) minimizes to the bar.
constexpr float kMinimizeX = 244;
constexpr float kMinimizeY = 112;

constexpr uint16_t kBarBackgroundNormal = 0x0803;
constexpr uint16_t kBarBackgroundWar = 0x0807;
constexpr uint16_t kBarLineRed = 0x0805;
constexpr uint16_t kBarLineBlue = 0x0806;
constexpr uint16_t kBarLinePoisoned = 0x0808;
constexpr uint16_t kBarLineYellow = 0x0809;
constexpr int kBarWidth = 109;

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

uo::world::SkillLock& statLock(uo::world::Player& p, int stat)
{
    switch (stat)
    {
    case 0:
        return p.strLock;
    case 1:
        return p.dexLock;
    default:
        return p.intLock;
    }
}

std::string pair(unsigned a, unsigned b)
{
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%u/%u", a, b);
    return buf;
}

std::string clilocOr(GumpContext& ctx, uint32_t number, const char* fallback)
{
    std::string s = ctx.text ? ctx.text->cliloc(number) : std::string();
    return s.empty() ? std::string(fallback) : s;
}

// BaseHealthBarGump.CalculatePercents.
int calculatePercents(int max, int current, int maxValue)
{
    if (max > 0)
    {
        max = current * 100 / max;

        if (max > 100)
        {
            max = 100;
        }

        if (max > 1)
        {
            max = maxValue * max / 100;
        }
    }

    return max;
}

GumpManager* managerOf(Gump* gump)
{
    return gump->manager();
}

uint32_t playerSerial(const uo::world::World& world)
{
    return world.player() ? world.player()->serial : 0;
}

}  // namespace

// --- StatusGump ------------------------------------------------------------------------

StatusGump::StatusGump(GumpContext& ctx, uo::world::World& world)
    : Gump(ctx, GumpKind::Status, playerSerial(world), 0), _world(world)
{
    init();
    autorelease();
    setCanMove(true);
    setCanCloseWithRightClick(true);

    auto* background = add(new GumpPic(ctx, kStatusBackground));
    background->setUOPosition(0, 0);

    GumpTextStyle style;
    style.font = 1;
    style.unicode = false;
    style.hue = kTextHue;

    auto label = [&](Stat stat, float x, float y) {
        auto* l = add(new TextLabel(ctx, "", style));
        l->setUOPosition(x, y);
        _labels[stat] = l;
    };

    label(Name, 86, 42);

    // Stat lockers (x 28 with UOP gumps, 40 with gumpart.mul; the classic client is MUL).
    static constexpr float kLockX = 40;
    static constexpr float kLockY[3] = {62, 74, 86};

    for (int i = 0; i < 3; ++i)
    {
        auto* locker = add(new StatusClickPic(ctx, kLockUp));
        locker->setUOPosition(kLockX, kLockY[i]);
        locker->setMovesGump(false);
        locker->onClicked = [this, i] { cycleStatLock(i); };
        _lockers[i] = locker;
    }

    label(Strength, 86, 62);
    label(Dexterity, 86, 74);
    label(Intelligence, 86, 86);
    label(Sex, 86, 98);
    label(Armor, 86, 110);
    label(Hits, 171, 62);
    label(Mana, 171, 74);
    label(Stamina, 171, 86);
    label(Gold, 171, 98);
    label(Weight, 171, 110);

    struct Box
    {
        float x, y, w;
        uint32_t cliloc;
        const char* fallback;
    };

    static constexpr Box kBoxes[] = {
        {86, 61, 34, 3000077, "Strength"},  {86, 73, 34, 3000078, "Dexterity"}, {86, 85, 34, 3000079, "Intelligence"},
        {86, 97, 34, 3000076, "Sex"},       {86, 109, 34, 1062760, "Armor"},    {171, 61, 66, 3000080, "Hits"},
        {171, 73, 66, 1061151, "Mana"},     {171, 85, 66, 1061150, "Stamina"},  {171, 97, 66, 1061156, "Gold"},
        {171, 109, 66, 1061154, "Weight"},
    };

    for (const Box& b : kBoxes)
    {
        add(new StatusHitBox(b.x, b.y, b.w, 12, clilocOr(ctx, b.cliloc, b.fallback)));
    }

    // StatusGumpBase.OnMouseUp: a click from _point to Width + 16 / Height + 16 opens the bar.
    const ax::Size bg = background->getContentSize();
    auto* corner = add(new StatusHitBox(kMinimizeX, kMinimizeY, std::max(0.0f, bg.width + 16 - kMinimizeX),
                                        std::max(0.0f, bg.height + 16 - kMinimizeY), std::string()));
    corner->onClicked = [this] { minimize(); };

    refresh();
}

void StatusGump::setValue(Stat stat, std::string text)
{
    if (_values[stat] == text)
    {
        return;
    }

    _values[stat] = std::move(text);
    _labels[stat]->setText(_values[stat]);
}

void StatusGump::showStatLocks()
{
    const uo::world::Player* p = _world.player();

    if (!p)
    {
        return;
    }

    const uo::world::SkillLock locks[3] = {p->strLock, p->dexLock, p->intLock};

    for (int i = 0; i < 3; ++i)
    {
        const auto shown = static_cast<uint8_t>(locks[i]);

        if (_shownLocks[i] != shown)
        {
            _shownLocks[i] = shown;
            _lockers[i]->setGraphic(lockGraphic(locks[i]));
        }
    }
}

void StatusGump::cycleStatLock(int stat)
{
    uo::world::Player* p = _world.player();

    if (!p)
    {
        return;
    }

    // Up -> Down -> Locked -> Up, sent at once and shown before the server confirms.
    uo::world::SkillLock& lock = statLock(*p, stat);
    lock = static_cast<uo::world::SkillLock>((static_cast<uint8_t>(lock) + 1) % 3);

    if (context().actions)
    {
        context().actions->setStatLock(static_cast<uint8_t>(stat), static_cast<uint8_t>(lock));
    }

    showStatLocks();
}

void StatusGump::refresh()
{
    const uo::world::Player* p = _world.player();

    if (!p || isClosed())
    {
        return;
    }

    setValue(Name, p->name);
    setValue(Strength, std::to_string(p->strength));
    setValue(Dexterity, std::to_string(p->dexterity));
    setValue(Intelligence, std::to_string(p->intelligence));
    setValue(Sex, p->isFemale ? "Female" : "Male");
    setValue(Armor, std::to_string(p->physicalResistance));
    setValue(Hits, pair(p->hits, p->hitsMax));
    setValue(Mana, pair(p->mana, p->manaMax));
    setValue(Stamina, pair(p->stamina, p->staminaMax));
    setValue(Gold, std::to_string(p->gold));
    setValue(Weight, pair(p->weight, p->weightMax));
    showStatLocks();
}

void StatusGump::minimize()
{
    GumpManager* manager = managerOf(this);

    if (!manager || isClosed())
    {
        return;
    }

    if (auto* old = manager->find<StatusBarGump>(serial()))
    {
        old->close();
    }

    manager->add(new StatusBarGump(context(), _world), screenPosition());
    close();
}

// --- StatusBarGump ---------------------------------------------------------------------

StatusBarGump::StatusBarGump(GumpContext& ctx, uo::world::World& world)
    : Gump(ctx, GumpKind::HealthBar, playerSerial(world), 0), _world(world)
{
    init();
    autorelease();
    setCanMove(true);
    setCanCloseWithRightClick(true);

    const uo::world::Player* p = _world.player();
    _warMode = p && p->inWarMode();

    _background = add(new StatusClickPic(ctx, _warMode ? kBarBackgroundWar : kBarBackgroundNormal));
    _background->setUOPosition(0, 0);
    _background->setContainsByBounds(true);
    _background->onDoubleClicked = [this] { maximize(); };

    static constexpr float kLineY[3] = {12, 25, 38};

    for (int i = 0; i < 3; ++i)
    {
        auto* back = add(new GumpPic(ctx, kBarLineRed));
        back->setUOPosition(34, kLineY[i]);
        back->setAcceptsInput(false);

        if (i == 0)
        {
            _hitsBack = back;
        }
    }

    for (int i = 0; i < 3; ++i)
    {
        _bars[i] = add(new StatusBarFill(ctx, kBarLineBlue));
        _bars[i]->setUOPosition(34, kLineY[i]);
    }

    if (p && ctx.actions)
    {
        ctx.actions->requestStatus(p->serial);
    }

    refresh();
}

void StatusBarGump::refresh()
{
    const uo::world::Player* p = _world.player();

    if (!p || isClosed())
    {
        return;
    }

    if (p->inWarMode() != _warMode)
    {
        _warMode = p->inWarMode();
        _background->setGraphic(_warMode ? kBarBackgroundWar : kBarBackgroundNormal);
    }

    _hitsBack->setVisible(p->hitsMax > 0);

    HitsLine line = HitsLine::Normal;

    if (p->isPoisoned(_world.clientVersion))
    {
        line = HitsLine::Poisoned;
    }
    else if (p->isYellowHits())
    {
        line = HitsLine::Yellow;
    }

    if (line != _hitsLine)
    {
        _hitsLine = line;
        _bars[0]->setGraphic(line == HitsLine::Poisoned ? kBarLinePoisoned
                             : line == HitsLine::Yellow ? kBarLineYellow
                                                        : kBarLineBlue);
    }

    const int widths[3] = {
        calculatePercents(p->hitsMax, p->hits, kBarWidth),
        calculatePercents(p->manaMax, p->mana, kBarWidth),
        calculatePercents(p->staminaMax, p->stamina, kBarWidth),
    };

    for (int i = 0; i < 3; ++i)
    {
        if (widths[i] != _shownWidths[i])
        {
            _shownWidths[i] = widths[i];
            _bars[i]->setWidth(widths[i]);
        }
    }
}

void StatusBarGump::maximize()
{
    GumpManager* manager = managerOf(this);

    if (!manager || isClosed())
    {
        return;
    }

    if (auto* old = manager->find<StatusGump>(serial()))
    {
        old->close();
    }

    manager->add(new StatusGump(context(), _world), screenPosition());
    close();
}

}  // namespace uo::client::gumps
