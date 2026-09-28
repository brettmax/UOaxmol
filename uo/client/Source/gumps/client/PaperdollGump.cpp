// SPDX-License-Identifier: BSD-2-Clause
// Part of the AxmolUO client. Ported from ClassicUO Game/UI/Gumps/PaperdollGump.cs (BuildGump,
// buttons, EquipmentSlot, minimise, OnMouseUp drop handling, UpdateContents) and
// Game/UI/Controls/PaperDollInteractable.cs (body gump, layer order, GetAnimID,
// GumpPicEquipment).
#include "gumps/client/PaperdollGump.h"

#include "gumps/GumpActions.h"
#include "gumps/GumpManager.h"
#include "gumps/client/ContainerItemSupport.h"

#include "uo/net/PacketWriter.h"
#include "uo/world/World.h"

#include <algorithm>

namespace uo::client::gumps
{

using uo::world::Item;
using uo::world::Layer;
using uo::world::Mobile;

namespace
{

constexpr uint16_t kPeaceModeGumps[3] = {0x07e5, 0x07e6, 0x07e7};
constexpr uint16_t kWarModeGumps[3] = {0x07e8, 0x07e9, 0x07ea};

constexpr uint16_t kOwnBackground = 0x07d0;
constexpr uint16_t kOtherBackground = 0x07d1;
constexpr uint16_t kMinimizedBackground = 0x07ee;
constexpr uint16_t kProfileScroll = 0x07d2;
constexpr uint16_t kVirtueMenu = 0x0071;
constexpr uint16_t kCombatBook = 0x2B34;
constexpr uint16_t kRacialBook = 0x2B28;
constexpr uint16_t kSlotBackground = 0x243A;
constexpr uint16_t kSlotFrame = 0x2344;
constexpr uint16_t kDefaultBackpackGump = 0xC4F6;

constexpr int kMaleGumpOffset = 50000;
constexpr int kFemaleGumpOffset = 60000;

constexpr int kButtonX = 185;
constexpr int kButtonY = 44;
constexpr int kButtonStep = 27;

// Virtue gump reply (GameActions.ReplyGump(player, 0x1CD, 1, [serial])).
constexpr uint32_t kVirtueGumpId = 0x000001CD;

const char* layerName(Layer layer)
{
    switch (layer)
    {
        case Layer::OneHanded: return "OneHanded";
        case Layer::TwoHanded: return "TwoHanded";
        case Layer::Shoes: return "Shoes";
        case Layer::Pants: return "Pants";
        case Layer::Helmet: return "Helmet";
        case Layer::Gloves: return "Gloves";
        case Layer::Ring: return "Ring";
        case Layer::Talisman: return "Talisman";
        case Layer::Necklace: return "Necklace";
        case Layer::Bracelet: return "Bracelet";
        case Layer::Tunic: return "Tunic";
        case Layer::Earrings: return "Earrings";
        case Layer::Arms: return "Arms";
        case Layer::Cloak: return "Cloak";
        case Layer::Robe: return "Robe";
        default: return "Unknown";
    }
}

// A gump picture with a double-click action (profile scroll, virtue menu, books, base).
class ActionPic : public GumpPic
{
public:
    ActionPic(GumpContext& ctx, uint16_t graphic, std::function<void()> onDouble)
        : GumpPic(ctx, graphic), _onDouble(std::move(onDouble))
    {
    }

    void onDoubleClick(MouseButton button) override
    {
        if (button == MouseButton::Left && _onDouble)
        {
            _onDouble();
        }
    }

private:
    std::function<void()> _onDouble;
};

// HitBox: an invisible rectangle that reacts to a left click.
class PaperdollClickArea : public Control
{
public:
    PaperdollClickArea(float width, float height, std::function<void()> onLeftClick)
        : _onLeftClick(std::move(onLeftClick))
    {
        init();
        autorelease();
        setAcceptsInput(true);
        setUOSize(width, height);
    }

    void onClick(MouseButton button) override
    {
        if (button == MouseButton::Left && _onLeftClick)
        {
            _onLeftClick();
        }
    }

private:
    std::function<void()> _onLeftClick;
};

Control* makeLayer()
{
    auto* layer = new Control();
    layer->init();
    layer->autorelease();
    return layer;
}

}  // namespace

// --- PaperdollEquipPic -----------------------------------------------------------------

PaperdollEquipPic::PaperdollEquipPic(PaperdollGump& owner) : GumpPic(owner.context(), 0), _owner(owner)
{
    setMovesGump(false);
    setVisible(false);
    setAcceptsInput(false);
}

void PaperdollEquipPic::assign(uint32_t serial, uint16_t gump, uint16_t hue, bool partial, Layer layer, bool canLift)
{
    if (serial != _serial)
    {
        cancelSingleClick(this);
    }

    _serial = serial;
    _layer = layer;
    _canLift = canLift;
    _active = true;
    // The body moves the gump; equipment does not (GumpPicEquipment.CanMove = false).
    setMovesGump(!uo::world::isValidSerial(serial));
    setTooltipSerial(uo::world::isValidSerial(serial) ? serial : 0);
    setGraphic(gump, hue, partial);
    setAcceptsInput(true);
    setVisible(true);
}

void PaperdollEquipPic::clear()
{
    cancelSingleClick(this);
    _serial = 0;
    _active = false;
    _canLift = false;
    setTooltipSerial(0);
    setAcceptsInput(false);
    setVisible(false);
}

bool PaperdollEquipPic::containsLocal(const ax::Vec2& local) const
{
    return _active && GumpPic::containsLocal(local);
}

void PaperdollEquipPic::onClick(MouseButton button)
{
    if (button != MouseButton::Left || !uo::world::isValidSerial(_serial))
    {
        return;
    }

    if (targetIfTargeting(_owner.world(), _serial))
    {
        return;
    }

    scheduleSingleClick(this, _owner.context(), _serial);
}

void PaperdollEquipPic::onDoubleClick(MouseButton button)
{
    if (button != MouseButton::Left || !uo::world::isValidSerial(_serial))
    {
        return;
    }

    cancelSingleClick(this);

    if (auto* actions = _owner.context().actions; actions && _owner.world().inGame())
    {
        actions->doubleClick(_serial);
    }
}

bool PaperdollEquipPic::beginDrag()
{
    if (!_canLift || !uo::world::isValidSerial(_serial))
    {
        return false;
    }

    cancelSingleClick(this);
    // Lifting a weapon should refresh the weapon abilities (PlayerMobile.UpdateAbilities);
    // the client does that when the equipment changes.
    return liftItem(_owner.context(), _owner.world(), &_owner, _serial);
}

// --- PaperdollSlot ---------------------------------------------------------------------

PaperdollSlot::PaperdollSlot(PaperdollGump& owner, Layer layer) : _owner(owner), _layer(layer)
{
    init();
    autorelease();
    setAcceptsInput(true);
    setUOSize(19, 20);

    auto* bg = new GumpPicTiled(owner.context(), kSlotBackground, 19, 20);
    bg->setAcceptsInput(false);
    addChild(bg);
    bg->setUOPosition(0, 0);

    auto* frame = new GumpPic(owner.context(), kSlotFrame);
    frame->setAcceptsInput(false);
    addChild(frame);
    frame->setUOPosition(0, 0);

    setTooltip(std::string(layerName(layer)) + " slot");
}

void PaperdollSlot::setItem(uint32_t serial, uint16_t graphic, uint16_t hue, bool canLift)
{
    _canLift = canLift;

    if (serial == _serial && graphic == _graphic && hue == _hue)
    {
        return;
    }

    if (serial != _serial)
    {
        cancelSingleClick(this);
    }

    _serial = serial;
    _graphic = graphic;
    _hue = hue;

    if (_art)
    {
        _art->removeFromParent();
        _art = nullptr;
    }

    if (serial == 0)
    {
        setTooltipSerial(0);
        setTooltip(std::string(layerName(_layer)) + " slot");
        return;
    }

    setTooltip({});
    setTooltipSerial(serial);

    GumpContext& ctx = _owner.context();
    const uint16_t h = hue & 0x3FFF;
    ax::Texture2D* tex = ctx.textures->art(graphic, h, h != 0 && ctx.textures->artIsPartialHue(graphic));

    if (!tex)
    {
        return;
    }

    // ItemGumpFixed: the opaque part of the art, shrunk into 18x18 and centred when smaller.
    const ax::Rect rect = realArtBounds(ctx, graphic);

    if (rect.size.width <= 0 || rect.size.height <= 0)
    {
        return;
    }

    constexpr float kCell = 18;
    float w = kCell, h2 = kCell, px = 0, py = 0;

    if (rect.size.width < kCell)
    {
        w = rect.size.width;
        px = static_cast<float>((static_cast<int>(kCell) >> 1) - (static_cast<int>(w) >> 1));
    }

    if (rect.size.height < kCell)
    {
        h2 = rect.size.height;
        py = static_cast<float>((static_cast<int>(kCell) >> 1) - (static_cast<int>(h2) >> 1));
    }

    auto* s = ax::Sprite::createWithTexture(tex, rect);
    s->setAnchorPoint(ax::Vec2(0, 1));
    s->setScaleX(w / rect.size.width);
    s->setScaleY(h2 / rect.size.height);
    s->setPosition(ax::Vec2(px, getContentSize().height - py));
    addChild(s);
    _art = s;
}

void PaperdollSlot::onClick(MouseButton button)
{
    if (button != MouseButton::Left || !uo::world::isValidSerial(_serial))
    {
        return;
    }

    if (targetIfTargeting(_owner.world(), _serial))
    {
        return;
    }

    scheduleSingleClick(this, _owner.context(), _serial);
}

void PaperdollSlot::onDoubleClick(MouseButton button)
{
    if (button != MouseButton::Left || !uo::world::isValidSerial(_serial))
    {
        return;
    }

    cancelSingleClick(this);

    if (_owner.world().target.isTargeting)
    {
        return;
    }

    if (auto* actions = _owner.context().actions)
    {
        actions->doubleClick(_serial);
    }
}

bool PaperdollSlot::beginDrag()
{
    if (!_canLift || !uo::world::isValidSerial(_serial))
    {
        return false;
    }

    cancelSingleClick(this);
    return liftItem(_owner.context(), _owner.world(), &_owner, _serial);
}

// --- PaperdollGump ---------------------------------------------------------------------

PaperdollGump::Hooks& PaperdollGump::hooks()
{
    static Hooks h;
    return h;
}

PaperdollGump::PaperdollGump(GumpContext& ctx, uo::world::World& world, uint32_t serial, bool canLift)
    : Gump(ctx, GumpKind::Paperdoll, serial, 0), _world(world), _canLift(canLift)
{
    init();
    autorelease();
    setCanMove(true);
    setCanCloseWithRightClick(true);
    build();
    refresh();
}

bool PaperdollGump::isOwn() const
{
    return _world.isPlayer(serial());
}

PaperdollButton* PaperdollGump::addButton(Button id, uint16_t normal, uint16_t pressed, uint16_t over, int row)
{
    auto* b = add(new PaperdollButton(context(), normal, pressed, over));
    b->setUOPosition(kButtonX, static_cast<float>(kButtonY + kButtonStep * row));
    b->setButtonId(static_cast<uint32_t>(id));
    b->onClicked = [this, id](GumpButton*) { onMenuButton(id); };
    return b;
}

void PaperdollGump::build()
{
    GumpContext& ctx = context();
    const bool own = isOwn();
    const bool showBooks = own && hooks().paperdollBooks;
    const bool showRacialBook = showBooks && _world.clientVersion >= uo::cv::CV_7000;

    auto restore = [this]() {
        if (_minimized)
        {
            setMinimized(false);
        }
    };

    auto profile = [this]() {
        if (auto* actions = context().actions)
        {
            actions->requestProfile(serial());
        }
    };

    if (own)
    {
        _base = add(new ActionPic(ctx, kOwnBackground, restore));

        addButton(Button::Help, 0x07ef, 0x07f0, 0x07f1, 0);
        addButton(Button::Options, 0x07d6, 0x07d7, 0x07d8, 1);
        addButton(Button::LogOut, 0x07d9, 0x07da, 0x07db, 2);

        if (_world.clientVersion < uo::cv::CV_500A)
        {
            addButton(Button::Journal, 0x07dc, 0x07dd, 0x07de, 3);
        }
        else
        {
            addButton(Button::Quests, 0x57b5, 0x57b7, 0x57b6, 3);
        }

        addButton(Button::Skills, 0x07df, 0x07e0, 0x07e1, 4);
        addButton(Button::Guild, 0x57b2, 0x57b4, 0x57b3, 5);

        const Mobile* mobile = _world.mobile(serial());
        _isWarMode = mobile && mobile->inWarMode();
        const uint16_t* g = _isWarMode ? kWarModeGumps : kPeaceModeGumps;
        _warButton = addButton(Button::PeaceWarToggle, g[0], g[1], g[2], 6);

        constexpr int kScrollsStep = 14;
        int profileX = 25;

        if (showRacialBook)
        {
            profileX += kScrollsStep;
        }

        add(new ActionPic(ctx, kProfileScroll, profile))->setUOPosition(static_cast<float>(profileX), 196);
        profileX += kScrollsStep;

        add(new ActionPic(ctx, kProfileScroll, []() {
            if (auto& open = hooks().openParty)
            {
                open();
            }
        }))->setUOPosition(static_cast<float>(profileX), 196);

        add(new PaperdollClickArea(16, 16, [this]() {
            if (!_minimized)
            {
                setMinimized(true);
            }
        }))->setUOPosition(228, 260);
    }
    else
    {
        _base = add(new ActionPic(ctx, kOtherBackground, restore));
        add(new ActionPic(ctx, kProfileScroll, profile))->setUOPosition(25, 196);
    }

    addButton(Button::Status, 0x07eb, 0x07ec, 0x07ed, 7);

    add(new ActionPic(ctx, kVirtueMenu, [this]() {
        const uo::world::Player* player = _world.player();

        if (!player || !context().send)
        {
            return;
        }

        context().send(uo::net::PacketWriter::variable(0xB1)
                           .u32(player->serial)
                           .u32(kVirtueGumpId)
                           .u32(1)
                           .u32(1)
                           .u32(serial())
                           .u32(0)
                           .finish());
    }))->setUOPosition(80, 4);

    // Side slots: left column, then right column.
    static constexpr Layer kLeft[] = {Layer::Helmet,   Layer::Earrings,  Layer::Necklace,
                                      Layer::Ring,     Layer::Bracelet,  Layer::Tunic,
                                      Layer::OneHanded, Layer::TwoHanded, Layer::Talisman};
    static constexpr Layer kRight[] = {Layer::Robe, Layer::Gloves, Layer::Pants, Layer::Arms, Layer::Cloak, Layer::Shoes};

    for (int i = 0; i < static_cast<int>(std::size(kLeft)); ++i)
    {
        auto* slot = add(new PaperdollSlot(*this, kLeft[i]));
        slot->setUOPosition(2, static_cast<float>(70 + 21 * i));
        _slots.push_back(slot);
    }

    for (int i = 0; i < static_cast<int>(std::size(kRight)); ++i)
    {
        auto* slot = add(new PaperdollSlot(*this, kRight[i]));
        slot->setUOPosition(162, static_cast<float>(70 + 21 * i));
        _slots.push_back(slot);
    }

    // The doll itself (PaperDollInteractable at 8, 19).
    _dollLayer = add(makeLayer());
    _dollLayer->setUOPosition(8, 19);

    if (showBooks)
    {
        add(new ActionPic(ctx, kCombatBook, []() {
            if (auto& open = hooks().openAbilitiesBook)
            {
                open();
            }
        }))->setUOPosition(156, 200);

        if (showRacialBook)
        {
            add(new ActionPic(ctx, kRacialBook, []() {
                if (auto& open = hooks().openRacialAbilitiesBook)
                {
                    open();
                }
            }))->setUOPosition(23, 200);
        }
    }

    // Name and title: ASCII font 1, hue 0x386, 185 wide.
    GumpTextStyle style;
    style.font = 1;
    style.unicode = false;
    style.hue = 0x0386;
    style.maxWidth = 185;
    _title = add(new TextLabel(ctx, "", style));
    _title->setUOPosition(39, 262);
}

void PaperdollGump::setMinimized(bool minimized)
{
    if (_minimized == minimized)
    {
        return;
    }

    _minimized = minimized;
    _base->setGraphic(minimized ? kMinimizedBackground : (isOwn() ? kOwnBackground : kOtherBackground));

    for (auto* c : controls())
    {
        if (c != _base)
        {
            c->setVisible(!minimized);
        }
    }
}

void PaperdollGump::onMenuButton(Button id)
{
    GumpContext& ctx = context();
    GumpActions* actions = ctx.actions;
    const uo::world::Player* player = _world.player();
    auto& h = hooks();

    switch (id)
    {
        case Button::Help:
            if (actions)
            {
                actions->requestHelp();
            }
            break;

        case Button::Options:
            if (h.openOptions)
            {
                h.openOptions();
            }
            break;

        case Button::LogOut:
            if (h.logOut)
            {
                h.logOut();
            }
            break;

        case Button::Journal:
            if (h.openJournal)
            {
                h.openJournal();
            }
            break;

        case Button::Quests:
            if (actions && player)
            {
                actions->requestQuestMenu(player->serial);
            }
            break;

        case Button::Skills:
            if (!player)
            {
                break;
            }

            if (h.openSkills)
            {
                h.openSkills(player->serial);
            }
            else if (actions)
            {
                actions->requestSkills(player->serial);
            }
            break;

        case Button::Guild:
            // Send_GuildMenuRequest.
            if (player && ctx.send)
            {
                ctx.send(uo::net::PacketWriter::variable(0xD7).u32(player->serial).u16(0x28).u8(0x0A).finish());
            }
            break;

        case Button::PeaceWarToggle:
            if (actions && player)
            {
                actions->setWarMode(!player->warMode);
            }
            break;

        case Button::Status:
            if (isOwn())
            {
                if (h.openStatus)
                {
                    h.openStatus(serial());
                }
                else if (actions)
                {
                    actions->requestStatus(serial());
                }
            }
            else if (h.openHealthBar)
            {
                h.openHealthBar(serial());
            }
            else if (actions)
            {
                actions->requestStatus(serial());
            }
            break;
    }
}

void PaperdollGump::updateWarButton()
{
    if (!_warButton)
    {
        return;
    }

    const Mobile* mobile = _world.mobile(serial());

    if (mobile && mobile->inWarMode() != _isWarMode)
    {
        _isWarMode = mobile->inWarMode();
        const uint16_t* g = _isWarMode ? kWarModeGumps : kPeaceModeGumps;
        _warButton->setGraphics(g[0], g[1], g[2]);
    }
}

uint32_t PaperdollGump::heldSerial()
{
    GumpManager* manager = gumpManagerOf(this);
    return manager && manager->heldItem() ? manager->heldItem()->serial : 0;
}

uint16_t PaperdollGump::equipGumpId(uint16_t mobileGraphic, uint16_t animId, bool female) const
{
    int offset = female ? kFemaleGumpOffset : kMaleGumpOffset;

    // Dead gargoyles wear their own shroud.
    if (_world.clientVersion >= uo::cv::CV_7000 && animId == 0x03CA && (mobileGraphic == 0x02B7 || mobileGraphic == 0x02B6))
    {
        animId = 0x0223;
    }

    // Body conversions (Bodyconv.def / equipconv.def) and tileart.uop appearances are not
    // ported: a Second Age client has neither.
    auto exists = [this](int id) { return id <= 0xFFFF && context().textures->gump(static_cast<uint16_t>(id)) != nullptr; };

    // IsAnimExistsInGump: fall back to the other gender's gump.
    if (!exists(animId + offset))
    {
        offset = female ? kMaleGumpOffset : kFemaleGumpOffset;
    }

    return static_cast<uint16_t>(animId + offset);
}

void PaperdollGump::collectDoll(std::vector<Shown>& out)
{
    out.clear();
    Mobile* mobile = _world.mobile(serial());

    if (!mobile)
    {
        return;
    }

    GumpContext& ctx = context();
    const uint16_t mg = mobile->graphic;
    uint16_t hue = mobile->hue & 0x3FFF;
    uint16_t body;

    if (mg == 0x0191 || mg == 0x0193)
    {
        body = 0x000D;
    }
    else if (mg == 0x025D)
    {
        body = 0x000E;
    }
    else if (mg == 0x025E)
    {
        body = 0x000F;
    }
    else if (mg == 0x029A || mg == 0x02B6)
    {
        body = 0x029A;
    }
    else if (mg == 0x029B || mg == 0x02B7)
    {
        body = 0x0299;
    }
    else if (mg == 0x04E5)
    {
        body = 0xC835;
    }
    else if (mg == 0x03DB)
    {
        body = 0x000C;
        hue = 0x03EA;
    }
    else if (mobile->isFemale)
    {
        body = 0x000D;
    }
    else
    {
        body = 0x000C;
    }

    out.push_back({0, body, hue, true, 0, false, 0});

    if (mg == 0x03DB)
    {
        out.push_back({0, 0xC72B, static_cast<uint16_t>(mobile->hue & 0x3FFF), true, 0, false, 0});
    }

    const paperdoll_order::AnimIdOf animIdOf = [&ctx](uint16_t graphic) { return itemTileInfo(ctx, graphic).animId; };
    const paperdoll_order::Graphics gfx = paperdoll_order::graphicsFromMobile(_world, *mobile, animIdOf);
    const bool altTorso = mobile->isFemale || paperdoll_order::isGargoyleBody(mg);
    const paperdoll_order::Order order = paperdoll_order::build(gfx, altTorso);
    paperdoll_order::Order layers{};
    const int count = paperdoll_order::filter(order, false, layers);

    const uint32_t held = heldSerial();
    const uo::world::Player* player = _world.player();
    const bool mayLift = _world.inGame() && player && !player->isDead() && (_canLift || isOwn());

    for (int i = 0; i < count; ++i)
    {
        const Layer layer = layers[i];
        const Item* item = _world.findItemByLayer(*mobile, layer);

        if (!item || item->serial == held)
        {
            continue;
        }

        if (paperdoll_order::isCovered(_world, *mobile, layer, animIdOf))
        {
            continue;
        }

        const ItemTileInfo info = itemTileInfo(ctx, item->graphic);
        Shown s;
        s.serial = item->serial;
        s.gump = equipGumpId(mg, info.animId, mobile->isFemale);
        s.hue = item->hue & 0x3FFF;
        s.partial = info.partialHue;
        s.layer = static_cast<uint8_t>(layer);
        s.canLift = mayLift && layer != Layer::Beard && layer != Layer::Hair;
        out.push_back(s);
    }

    // The backpack is drawn last, over everything.
    const Item* pack = _world.findItemByLayer(*mobile, Layer::Backpack);

    if (pack && pack->serial != held)
    {
        const uint16_t animId = itemTileInfo(ctx, pack->graphic).animId;

        if (animId != 0)
        {
            uint16_t gump = static_cast<uint16_t>(animId + kMaleGumpOffset);

            // The player's backpack uses the newer backpack gump when the client has it
            // (BackpackStyle 0).
            if (isOwn() && ctx.textures->gump(kDefaultBackpackGump) != nullptr)
            {
                gump = kDefaultBackpackGump;
            }

            Shown s;
            s.serial = pack->serial;
            s.gump = gump;
            s.hue = pack->hue & 0x3FFF;
            s.layer = static_cast<uint8_t>(Layer::Backpack);
            s.x = hooks().paperdollBooks ? -6 : 0;
            out.push_back(s);
        }
    }
}

void PaperdollGump::syncDoll()
{
    collectDoll(_scratch);

    if (_shownValid && _scratch == _shown)
    {
        return;
    }

    while (_dollPics.size() < _scratch.size())
    {
        auto* pic = new PaperdollEquipPic(*this);
        _dollLayer->addChild(pic);
        pic->setUOPosition(0, 0);
        _dollPics.push_back(pic);
    }

    for (size_t i = 0; i < _dollPics.size(); ++i)
    {
        PaperdollEquipPic* pic = _dollPics[i];

        if (i >= _scratch.size())
        {
            pic->clear();
            continue;
        }

        const Shown& s = _scratch[i];

        if (!_shownValid || i >= _shown.size() || !(_shown[i] == s))
        {
            pic->assign(s.serial, s.gump, s.hue, s.partial, static_cast<Layer>(s.layer), s.canLift);
        }

        pic->setUOPosition(static_cast<float>(s.x), 0);
    }

    _shown.swap(_scratch);
    _shownValid = true;
}

void PaperdollGump::syncSlots()
{
    Mobile* mobile = _world.mobile(serial());
    const uo::world::Player* player = _world.player();
    const bool mayLift = _world.inGame() && player && (player->serial == serial() || _canLift);
    const uint32_t held = heldSerial();

    for (auto* slot : _slots)
    {
        const Item* item = mobile ? _world.findItemByLayer(*mobile, slot->layer()) : nullptr;

        if (!item || item->serial == held)
        {
            slot->setItem(0, 0, 0, false);
        }
        else
        {
            slot->setItem(item->serial, displayedGraphic(*item), item->hue, mayLift);
        }
    }
}

void PaperdollGump::refresh()
{
    if (isClosed())
    {
        return;
    }

    const Mobile* mobile = _world.mobile(serial());

    if (!mobile)
    {
        close();
        return;
    }

    updateWarButton();

    if (mobile->title != _titleText)
    {
        _titleText = mobile->title;
        _title->setText(_titleText);
    }

    syncDoll();
    syncSlots();
}

bool PaperdollGump::onDropItem(const HeldItem& held, Control* target, const ax::Vec2& /*local*/)
{
    GumpContext& ctx = context();
    GumpManager* manager = gumpManagerOf(this);
    const uo::world::Player* player = _world.player();

    if (!ctx.actions || !manager || !player || !_world.inGame())
    {
        return false;
    }

    if (!_canLift && !isOwn())
    {
        return true;
    }

    Mobile* mobile = _world.mobile(serial());

    if (!mobile)
    {
        return true;
    }

    // Onto the backpack (or any container) on the doll: into it.
    uint32_t targetSerial = 0;

    if (auto* pic = dynamic_cast<PaperdollEquipPic*>(target); pic && pic->isVisible())
    {
        targetSerial = pic->serial();
    }
    else if (auto* slot = dynamic_cast<PaperdollSlot*>(target))
    {
        targetSerial = slot->serial();
    }

    if (const Item* t = uo::world::isValidSerial(targetSerial) ? _world.item(targetSerial) : nullptr)
    {
        if (t->layer == Layer::Backpack || itemTileInfo(ctx, t->graphic).container)
        {
            if (t->serial != held.serial)
            {
                ctx.actions->dropToContainer(held.serial, t->serial, 0xFFFF, 0xFFFF);
                manager->setHeldItem(std::nullopt);
            }

            return true;
        }
    }

    // Otherwise a wearable goes on its tiledata layer, if that layer is free.
    const ItemTileInfo info = itemTileInfo(ctx, held.graphic);

    if (info.wearable && info.layer != 0 && !_world.findItemByLayer(*mobile, static_cast<Layer>(info.layer)))
    {
        ctx.actions->equip(held.serial, info.layer, isOwn() ? player->serial : mobile->serial);
        manager->setHeldItem(std::nullopt);
    }

    return true;
}

}  // namespace uo::client::gumps
