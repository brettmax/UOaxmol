// SPDX-License-Identifier: BSD-2-Clause
// Ported from ClassicUO (ClassicUO.Client/Network/PacketHandlers.cs): speech, effects,
// environment, targeting, trade, shops, books, menus, chat, 0xBF sub-commands and the other
// handlers whose ClassicUO versions open gumps. Here they update state and notify the
// WorldListener; the Axmol UI decides what to show.
#include "PacketHandlersInternal.h"
#include "uo/io/Compression.h"
#include "uo/world/PacketHandlers.h"

#include <algorithm>
#include <cctype>
#include <charconv>

namespace uo::world
{

using io::BinaryReader;

namespace
{

bool iequals(std::string_view a, std::string_view b)
{
    return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(), [](char x, char y) {
               return std::tolower(static_cast<unsigned char>(x)) == std::tolower(static_cast<unsigned char>(y));
           });
}

bool parseUInt(std::string_view s, uint32_t& out)
{
    if (s.empty())
        return false;
    auto [p, ec] = std::from_chars(s.data(), s.data() + s.size(), out);
    return ec == std::errc{} && p == s.data() + s.size();
}

// Shared tail of Talk / UnicodeTalk / DisplayClilocString: classify the message and name
// unnamed speakers.
TextType classify(World& world, Entity* entity, Serial serial, MessageType type, const std::string& name,
                  const std::string& text)
{
    if (type == MessageType::Alliance || type == MessageType::Guild)
        return TextType::GuildAlly;

    if (type == MessageType::System || serial == 0xFFFFFFFFu || serial == 0 || (iequals(name, "system") && !entity))
        return TextType::System;

    if (entity)
    {
        if (entity->name.empty())
        {
            entity->name = name.empty() ? text : name;
            world.listener().onNameChanged(*entity);
        }
        return TextType::Object;
    }

    return TextType::System;
}

void printSystem(World& world, std::string text, uint16_t hue, MessageType type = MessageType::System,
                 uint16_t font = 3)
{
    Message msg;
    msg.text = std::move(text);
    msg.hue = hue;
    msg.type = type;
    msg.font = font;
    msg.textType = TextType::Client;
    world.addMessage(std::move(msg));
}

} // namespace

// ---------------------------------------------------------------------------------------------
// Speech
// ---------------------------------------------------------------------------------------------

void PacketHandlers::talk(PacketHandlers&, World& world, BinaryReader& r)
{
    Message msg;
    msg.serial = r.readU32BE();
    Entity* entity = world.get(msg.serial);
    msg.graphic = r.readU16BE();
    msg.type = MessageType(r.readU8());
    msg.hue = r.readU16BE();
    msg.font = r.readU16BE();
    msg.name = detail::ascii(r, 30);

    if (r.size() > 44)
    {
        r.seek(44);
        msg.text = detail::ascii(r, -1);
    }

    if (msg.serial == 0 && msg.graphic == 0 && msg.type == MessageType::Regular && msg.font == 0xFFFF &&
        msg.hue == 0xFFFF && msg.name.starts_with("SYSTEM"))
    {
        world.requests().ackTalk();
        return;
    }

    msg.textType = msg.type == MessageType::System || msg.serial == 0xFFFFFFFFu || msg.serial == 0 ||
                           (iequals(msg.name, "system") && !entity)
                       ? TextType::System
                       : classify(world, entity, msg.serial, msg.type, msg.name, msg.text);
    // ASCII talk never reports guild/alliance as its own text type.
    if (msg.textType == TextType::GuildAlly)
        msg.textType = entity ? TextType::Object : TextType::System;

    world.addMessage(std::move(msg));
}

void PacketHandlers::unicodeTalk(PacketHandlers&, World& world, BinaryReader& r)
{
    if (!world.inGame())
        return; // ClassicUO only logs these during login

    Message msg;
    msg.unicode = true;
    msg.serial = r.readU32BE();
    Entity* entity = world.get(msg.serial);
    msg.graphic = r.readU16BE();
    msg.type = MessageType(r.readU8());
    msg.hue = r.readU16BE();
    msg.font = r.readU16BE();
    msg.lang = detail::ascii(r, 4);
    msg.name = detail::ascii(r, -1);

    if (msg.serial == 0 && msg.graphic == 0 && msg.type == MessageType::Regular && msg.font == 0xFFFF &&
        msg.hue == 0xFFFF && iequals(msg.name, "system"))
    {
        world.requests().ackUnicodeSystemTalk();
        return;
    }

    if (r.size() > 48)
    {
        r.seek(48);
        msg.text = detail::unicodeBE(r, -1);
    }

    msg.textType = classify(world, entity, msg.serial, msg.type, msg.name, msg.text);
    world.addMessage(std::move(msg));
}

void PacketHandlers::displayClilocString(PacketHandlers&, World& world, BinaryReader& r)
{
    if (!world.player())
        return;

    constexpr uint8_t kPrepend = 0x01;
    constexpr uint8_t kSystem = 0x02;

    const bool isAffix = r.buffer()[0] == 0xCC;

    Message msg;
    msg.unicode = true;
    msg.serial = r.readU32BE();
    Entity* entity = world.get(msg.serial);
    msg.graphic = r.readU16BE();
    msg.type = MessageType(r.readU8());
    msg.hue = r.readU16BE();
    msg.font = r.readU16BE();
    msg.cliloc = r.readU32BE();
    const uint8_t affixFlags = isAffix ? r.readU8() : 0;
    msg.name = detail::ascii(r, 30);
    const std::string affix = isAffix ? detail::ascii(r, -1) : std::string{};

    std::string arguments;
    if (const size_t remains = r.remaining(); remains > 0)
        arguments = isAffix ? detail::unicodeBE(r, int(remains / 2)) : detail::unicodeLE(r, int(remains / 2));

    msg.clilocArgs = arguments;
    msg.affix = affix;
    msg.affixPrepend = (affixFlags & kPrepend) != 0;

    // With a resolver the text is ready to show; without one the text layer resolves it later.
    if (ClilocResolver* clilocs = world.clilocs())
    {
        auto text = clilocs->translate(msg.cliloc, arguments, false);
        if (!text)
            return;
        msg.text = std::move(*text);

        if (!affix.empty() && affix.find_first_not_of(" \t\r\n") != std::string::npos)
            msg.text = msg.affixPrepend ? affix + msg.text : msg.text + affix;
    }

    if (affixFlags & kSystem)
        msg.type = MessageType::System;

    if (msg.serial == 0xFFFFFFFFu || msg.serial == 0 || iequals(msg.name, "system"))
    {
        msg.textType = TextType::System;
    }
    else if (entity)
    {
        msg.textType = TextType::Object;
        if (entity->name.empty())
        {
            entity->name = msg.name;
            world.listener().onNameChanged(*entity);
        }
    }
    else
    {
        if (msg.type == MessageType::Label)
            return;
        msg.textType = TextType::System;
    }

    world.addMessage(std::move(msg));
}

void PacketHandlers::asciiPrompt(PacketHandlers&, World& world, BinaryReader& r)
{
    if (!world.inGame())
        return;
    world.prompt = {PromptKind::ASCII, r.readU64BE()};
    world.listener().onPromptChanged(world.prompt);
}

void PacketHandlers::unicodePrompt(PacketHandlers&, World& world, BinaryReader& r)
{
    if (!world.inGame())
        return;
    world.prompt = {PromptKind::Unicode, r.readU64BE()};
    world.listener().onPromptChanged(world.prompt);
}

// ---------------------------------------------------------------------------------------------
// Effects, audio, environment
// ---------------------------------------------------------------------------------------------

void PacketHandlers::dragAnimation(PacketHandlers&, World& world, BinaryReader& r)
{
    Effect e;
    e.graphic = r.readU16BE();
    e.graphic = uint16_t(e.graphic + r.readU8());
    e.hue = r.readU16BE();
    r.readU16BE(); // count
    e.source = r.readU32BE();
    e.sourceX = r.readU16BE();
    e.sourceY = r.readU16BE();
    e.sourceZ = r.readI8();
    e.target = r.readU32BE();
    e.targetX = r.readU16BE();
    e.targetY = r.readU16BE();
    e.targetZ = r.readI8();

    // Coins animate as a pile rather than a single coin.
    if (e.graphic == 0x0EED)
        e.graphic = 0x0EEF;
    else if (e.graphic == 0x0EEA)
        e.graphic = 0x0EEC;
    else if (e.graphic == 0x0EF0)
        e.graphic = 0x0EF2;

    if (Mobile* m = world.mobile(e.source))
    {
        e.sourceX = m->x;
        e.sourceY = m->y;
        e.sourceZ = m->z;
    }
    else
    {
        e.source = 0;
    }

    if (Mobile* m = world.mobile(e.target))
    {
        e.targetX = m->x;
        e.targetY = m->y;
        e.targetZ = m->z;
    }
    else
    {
        e.target = 0;
    }

    e.type = !isValidSerial(e.source) || !isValidSerial(e.target) ? GraphicEffectType::Moving
                                                                   : GraphicEffectType::DragEffect;
    e.speed = 5;
    e.duration = 5000;
    e.fixedDirection = true;
    world.listener().onEffect(e);
}

void PacketHandlers::graphicEffect(PacketHandlers&, World& world, BinaryReader& r)
{
    if (!world.player())
        return;

    const uint8_t id = r.buffer()[0];
    Effect e;
    e.type = GraphicEffectType(r.readU8());

    if (e.type > GraphicEffectType::FixedFrom)
        return; // ScreenFade is not implemented in ClassicUO either

    e.source = r.readU32BE();
    e.target = r.readU32BE();
    e.graphic = r.readU16BE();
    e.sourceX = r.readU16BE();
    e.sourceY = r.readU16BE();
    e.sourceZ = r.readI8();
    e.targetX = r.readU16BE();
    e.targetY = r.readU16BE();
    e.targetZ = r.readI8();
    e.speed = r.readU8();
    e.duration = r.readU8();
    r.readU16BE(); // unknown
    e.fixedDirection = (r.readU8() != 0);
    e.explode = (r.readU8() != 0);

    if (id != 0x70)
    {
        e.hue = uint16_t(r.readU32BE());
        e.blendMode = GraphicEffectBlendMode(r.readU32BE() % 7);

        if (id == 0xC7)
        {
            e.tileId = r.readU16BE();
            e.explodeEffect = r.readU16BE();
            e.explodeSound = r.readU16BE();
            e.effectSerial = r.readU32BE();
            e.layer = r.readU8();
            r.skip(2);
        }
    }

    world.listener().onEffect(e);
}

void PacketHandlers::playSoundEffect(PacketHandlers&, World& world, BinaryReader& r)
{
    if (!world.player())
        return;

    SoundRequest s;
    s.mode = r.readU8();
    s.index = r.readU16BE();
    s.volume = r.readU16BE();
    s.x = r.readU16BE();
    s.y = r.readU16BE();
    s.z = r.readI16BE();
    world.listener().onSound(s);
}

void PacketHandlers::playMusic(PacketHandlers&, World& world, BinaryReader& r)
{
    if (r.size() == 3)
    {
        const uint8_t cmd = r.readU8();
        const uint8_t index = r.readU8();
        world.listener().onMusic(cmd == 0x1F && index == 0xFF ? -1 : int(index));
    }
    else
    {
        world.listener().onMusic(r.readU16BE());
    }
}

void PacketHandlers::personalLightLevel(PacketHandlers&, World& world, BinaryReader& r)
{
    if (!world.inGame())
        return;

    if (world.isPlayer(r.readU32BE()))
    {
        const uint8_t level = std::min<uint8_t>(r.readU8(), 0x1E);
        world.light.realPersonal = level;
        world.light.personal = level;
        world.listener().onLightChanged();
    }
}

void PacketHandlers::lightLevel(PacketHandlers&, World& world, BinaryReader& r)
{
    if (!world.inGame())
        return;

    const uint8_t level = std::min<uint8_t>(r.readU8(), 0x1E);
    world.light.realOverall = level;
    world.light.overall = level; // a custom light level override is applied by the client
    world.listener().onLightChanged();
}

void PacketHandlers::setWeather(PacketHandlers&, World& world, BinaryReader& r)
{
    const auto type = WeatherType(r.readU8());
    if (world.weather.current == type)
        return;

    const uint8_t count = r.readU8();
    const uint8_t temperature = r.readU8();

    // Weather.Generate
    auto& w = world.weather;
    const bool extended = w.current.has_value() && *w.current == type;
    if (!extended)
        w.reset();

    w.type = type;
    w.count = std::min<uint8_t>(70, count);
    w.temperature = temperature;

    if (type == WeatherType::Invalid0 || type == WeatherType::Invalid1)
    {
        w.current.reset();
        world.listener().onWeatherChanged();
        return;
    }

    if (w.count > 0 && !extended)
    {
        const char* text = nullptr;
        switch (type)
        {
        case WeatherType::Rain: text = "It begins to rain."; break;
        case WeatherType::StormApproach: text = "A fierce storm approaches."; break;
        case WeatherType::Snow: text = "It begins to snow."; break;
        case WeatherType::StormBrewing: text = "A storm is brewing."; break;
        default: break;
        }
        if (text)
        {
            printSystem(world, text, 1154);
            w.current = type;
        }
    }

    world.listener().onWeatherChanged();
}

void PacketHandlers::season(PacketHandlers&, World& world, BinaryReader& r)
{
    Player* p = world.player();
    if (!p)
        return;

    uint8_t s = r.readU8();
    const uint8_t music = r.readU8();
    if (s > 4)
        s = 0;

    if (p->isDead() && s == 4)
        return;

    world.oldSeason = Season(s);
    world.oldMusicIndex = music;
    if (world.season == Season::Desolation)
        world.oldMusicIndex = 42;

    world.changeSeason(Season(s), music);
}

void PacketHandlers::deathScreen(PacketHandlers&, World& world, BinaryReader& r)
{
    if (r.readU8() != 1)
    {
        world.weather.reset();
        world.listener().onWeatherChanged();
        world.listener().onDeathScreen();
    }
}

void PacketHandlers::ping(PacketHandlers&, World& world, BinaryReader& r)
{
    world.listener().onPing(r.readU8());
}

void PacketHandlers::clientViewRange(PacketHandlers&, World& world, BinaryReader& r)
{
    world.clientViewRange = r.readU8();
}

// ---------------------------------------------------------------------------------------------
// Targeting
// ---------------------------------------------------------------------------------------------

void PacketHandlers::targetCursor(PacketHandlers&, World& world, BinaryReader& r)
{
    const auto state = CursorTarget(int8_t(r.readU8()));
    const uint32_t cursorId = r.readU32BE();
    const auto type = TargetType(r.readU8());
    world.setTargeting(state, cursorId, type);
}

void PacketHandlers::multiPlacement(PacketHandlers&, World& world, BinaryReader& r)
{
    if (!world.player())
        return;

    r.skip(1); // allow ground
    const uint32_t targetId = r.readU32BE();
    r.readU8(); // flags
    r.seek(18);
    const uint16_t multiId = r.readU16BE();
    const uint16_t xOff = r.readU16BE();
    const uint16_t yOff = r.readU16BE();
    const uint16_t zOff = r.readU16BE();
    const uint16_t hue = r.readU16BE();

    world.setTargetingMulti(targetId, multiId, xOff, yOff, zOff, hue);
}

// ---------------------------------------------------------------------------------------------
// Trade and shops
// ---------------------------------------------------------------------------------------------

void PacketHandlers::secureTrading(PacketHandlers&, World& world, BinaryReader& r)
{
    if (!world.inGame())
        return;

    TradeEvent t;
    const uint8_t type = r.readU8();
    t.kind = TradeEvent::Kind(type);
    t.container = r.readU32BE();

    switch (type)
    {
    case 0:
    {
        t.myBox = r.readU32BE();
        t.theirBox = r.readU32BE();
        // The standard client refuses trades with anyone it cannot see.
        if (!world.get(t.myBox) || !world.get(t.theirBox))
            return;
        if ((r.readU8() != 0) && r.position() < r.size())
            t.theirName = detail::ascii(r, -1);
        break;
    }
    case 1: break;
    case 2:
        t.iAccept = r.readU32BE() != 0;
        t.theyAccept = r.readU32BE() != 0;
        break;
    case 3:
    case 4:
        t.gold = r.readU32BE();
        t.platinum = r.readU32BE();
        break;
    default: return;
    }

    world.listener().onTrade(t);
}

void PacketHandlers::buyList(PacketHandlers&, World& world, BinaryReader& r)
{
    if (!world.inGame())
        return;

    Item* container = world.item(r.readU32BE());
    if (!container || !world.mobile(container->container))
        return;

    if (container->layer != Layer::ShopBuyRestock && container->layer != Layer::ShopBuy)
        return;

    const uint8_t count = r.readU8();
    if (container->isEmpty())
        return;

    std::vector<Serial> order = container->contents();
    if (container->graphic == 0x2AF8)
    {
        // Hard-coded in the original client: this box is listed sorted by x.
        std::stable_sort(order.begin(), order.end(), [&world](Serial a, Serial b) {
            const Item* ia = world.item(a);
            const Item* ib = world.item(b);
            return (ia ? ia->x : 0) < (ib ? ib->x : 0);
        });
    }
    else
    {
        std::reverse(order.begin(), order.end());
    }

    for (uint8_t i = 0; i < count && i < order.size(); ++i)
    {
        Item* it = world.item(order[i]);
        const uint32_t price = r.readU32BE();
        const uint8_t nameLen = r.readU8();
        std::string name = detail::ascii(r, nameLen);
        if (!it)
            continue;

        it->price = price;
        uint32_t cliloc = 0;
        if (const ObjectProperty* opl = world.properties(it->serial))
            it->name = opl->name;
        else if (parseUInt(name, cliloc))
            it->name = detail::clilocOr(world, cliloc, name);
        else
            it->name = std::move(name); // empty: the UI falls back to the tiledata name
    }
}

void PacketHandlers::sellList(PacketHandlers&, World& world, BinaryReader& r)
{
    if (!world.inGame())
        return;

    const Serial vendor = r.readU32BE();
    if (!world.mobile(vendor))
        return;

    const uint16_t count = r.readU16BE();
    if (count == 0)
        return;

    ShopData shop;
    shop.vendor = vendor;
    shop.isBuy = false;

    for (uint16_t i = 0; i < count; ++i)
    {
        ShopEntry e;
        e.serial = r.readU32BE();
        e.graphic = r.readU16BE();
        e.hue = r.readU16BE();
        e.amount = r.readU16BE();
        e.price = r.readU16BE();
        e.name = detail::ascii(r, r.readU16BE());

        if (parseUInt(e.name, e.cliloc))
        {
            e.nameIsCliloc = true;
            e.name = detail::clilocOr(world, e.cliloc, e.name);
        }
        else if (e.name.empty())
        {
            if (const ObjectProperty* opl = world.properties(e.serial))
                e.name = opl->name;
        }

        shop.entries.push_back(std::move(e));
    }

    world.listener().onOpenShop(shop);
}

void PacketHandlers::closeVendorInterface(PacketHandlers&, World& world, BinaryReader& r)
{
    if (world.inGame())
        world.listener().onCloseShop(r.readU32BE());
}

// ---------------------------------------------------------------------------------------------
// Books, boards, menus, dialogs
// ---------------------------------------------------------------------------------------------

void PacketHandlers::openBook(PacketHandlers& self, World& world, BinaryReader& r)
{
    BookHeader book;
    book.serial = r.readU32BE();
    const bool oldPacket = r.buffer()[0] == 0x93;
    book.isNewPacket = !oldPacket;
    book.editable = (r.readU8() != 0);
    if (!oldPacket)
        book.editable = (r.readU8() != 0);
    else
        r.skip(1);

    book.pageCount = r.readU16BE();
    book.title = oldPacket ? detail::utf8(r, 60) : detail::utf8(r, r.readU16BE());
    book.author = oldPacket ? detail::utf8(r, 30) : detail::utf8(r, r.readU16BE());

    const bool alreadyOpen = self._openBooks.contains(book.serial);
    self._openBooks[book.serial] = book.isNewPacket;

    world.listener().onBookOpen(book);
    if (!alreadyOpen)
        world.requests().bookPageRequest(book.serial, 1);
}

void PacketHandlers::bookData(PacketHandlers& self, World& world, BinaryReader& r)
{
    if (!world.inGame())
        return;

    BookContent content;
    content.serial = r.readU32BE();
    const uint16_t pageCount = r.readU16BE();

    auto it = self._openBooks.find(content.serial);
    const bool isNewBook = it != self._openBooks.end() && it->second;

    for (uint16_t i = 0; i < pageCount; ++i)
    {
        BookPage page;
        page.page = int(r.readU16BE()) - 1;
        const uint16_t lineCount = r.readU16BE();
        for (uint16_t line = 0; line < lineCount; ++line)
            page.lines.push_back(isNewBook ? detail::utf8(r, -1) : detail::ascii(r, -1));
        content.pages.push_back(std::move(page));
    }

    world.listener().onBookContent(content);
}

void PacketHandlers::bulletinBoardData(PacketHandlers&, World& world, BinaryReader& r)
{
    if (!world.inGame())
        return;

    BulletinEvent ev;
    const uint8_t kind = r.readU8();

    switch (kind)
    {
    case 0: // open
    {
        ev.kind = BulletinEvent::Kind::Open;
        ev.board = r.readU32BE();
        Item* board = world.item(ev.board);
        if (!board)
            return;
        board->opened = true;
        ev.boardName = detail::utf8(r, 22);
        break;
    }
    case 1: // summary
    {
        ev.kind = BulletinEvent::Kind::Summary;
        ev.board = r.readU32BE();
        ev.message = r.readU32BE();
        ev.parent = r.readU32BE();
        ev.poster = detail::utf8(r, r.readU8());
        ev.subject = detail::utf8(r, r.readU8());
        ev.time = detail::utf8(r, r.readU8());
        break;
    }
    case 2: // message
    {
        ev.kind = BulletinEvent::Kind::Message;
        ev.board = r.readU32BE();
        ev.message = r.readU32BE();
        ev.poster = detail::ascii(r, r.readU8());
        ev.subject = detail::utf8(r, r.readU8());
        ev.time = detail::ascii(r, r.readU8());
        r.skip(4);
        const uint8_t unk = r.readU8();
        r.skip(size_t(unk) * 4);
        const uint8_t lines = r.readU8();
        for (uint8_t i = 0; i < lines; ++i)
        {
            const uint8_t len = r.readU8();
            if (len > 0)
            {
                ev.body += detail::utf8(r, len);
                ev.body += '\n';
            }
        }
        const size_t start = ev.body.find_first_not_of(" \t\r\n");
        ev.body = start == std::string::npos ? std::string{} : ev.body.substr(start);
        break;
    }
    default: return;
    }

    world.listener().onBulletinBoard(ev);
}

void PacketHandlers::openMenu(PacketHandlers&, World& world, BinaryReader& r)
{
    if (!world.inGame())
        return;

    MenuData menu;
    menu.serial = r.readU32BE();
    menu.menuId = r.readU16BE();
    menu.title = detail::ascii(r, r.readU8());
    const uint8_t count = r.readU8();

    // A question menu has no item graphics: its first entry starts with a zero model id.
    const size_t pos = r.position();
    const uint16_t firstModel = r.readU16BE();
    r.seek(pos);
    menu.isGray = firstModel == 0;

    for (uint8_t i = 0; i < count; ++i)
    {
        MenuEntry e;
        if (menu.isGray)
        {
            r.skip(4);
        }
        else
        {
            e.graphic = r.readU16BE();
            e.hue = r.readU16BE();
        }
        e.text = detail::ascii(r, r.readU8());
        menu.entries.push_back(std::move(e));
    }

    world.listener().onMenu(menu);
}

void PacketHandlers::openPaperdoll(PacketHandlers&, World& world, BinaryReader& r)
{
    Mobile* m = world.mobile(r.readU32BE());
    if (!m)
        return;

    m->title = detail::ascii(r, 60);
    const uint8_t flagBits = r.readU8();
    world.listener().onOpenPaperdoll(*m, m->title, (flagBits & 0x02) != 0);
}

void PacketHandlers::displayMap(PacketHandlers&, World& world, BinaryReader& r)
{
    MapDisplay map;
    map.serial = r.readU32BE();
    map.gumpId = r.readU16BE();
    map.startX = r.readU16BE();
    map.startY = r.readU16BE();
    map.endX = r.readU16BE();
    map.endY = r.readU16BE();
    map.width = r.readU16BE();
    map.height = r.readU16BE();

    if (r.buffer()[0] == 0xF5)
        map.facet = r.readU16BE();
    else if (world.clientVersion >= versions::CV_308Z)
        map.facet = uint16_t(0);

    if (Item* it = world.item(map.serial))
        it->opened = true;

    world.listener().onMapDisplay(map);
}

void PacketHandlers::mapData(PacketHandlers&, World& world, BinaryReader& r)
{
    if (!world.inGame())
        return;

    MapPinMessage m;
    m.serial = r.readU32BE();
    m.command = r.readU8();

    if (m.command == 1) // add
    {
        r.skip(1);
        m.x = r.readU16BE();
        m.y = r.readU16BE();
    }
    else if (m.command == 7) // edit response
    {
        m.plotState = r.readU8();
    }

    world.listener().onMapPin(m);
}

void PacketHandlers::dyeData(PacketHandlers&, World& world, BinaryReader& r)
{
    const Serial serial = r.readU32BE();
    r.skip(2);
    world.listener().onDyeWindow(serial, r.readU16BE());
}

void PacketHandlers::openUrl(PacketHandlers&, World& world, BinaryReader& r)
{
    std::string url = detail::ascii(r, -1);
    if (!url.empty())
        world.listener().onOpenUrl(url);
}

void PacketHandlers::tipWindow(PacketHandlers&, World& world, BinaryReader& r)
{
    const uint8_t flag = r.readU8();
    if (flag == 1)
        return;

    const uint32_t tip = r.readU32BE();
    std::string text = detail::ascii(r, r.readU16BE());
    std::replace(text.begin(), text.end(), '\r', '\n');
    world.listener().onTip(tip, flag, text);
}

void PacketHandlers::textEntryDialog(PacketHandlers&, World& world, BinaryReader& r)
{
    if (!world.inGame())
        return;

    TextEntryDialog d;
    d.serial = r.readU32BE();
    d.parentId = r.readU8();
    d.buttonId = r.readU8();
    d.text = detail::ascii(r, r.readU16BE());
    d.canCancel = (r.readU8() != 0);
    d.variant = r.readU8();
    d.maxLength = r.readU32BE();
    d.description = detail::ascii(r, r.readU16BE());
    world.listener().onTextEntryDialog(d);
}

void PacketHandlers::characterProfile(PacketHandlers&, World& world, BinaryReader& r)
{
    if (!world.inGame())
        return;

    const Serial serial = r.readU32BE();
    std::string header = detail::ascii(r, -1);
    std::string footer = detail::unicodeBE(r, -1);
    std::string body = detail::unicodeBE(r, -1);
    world.listener().onProfile(serial, header, footer, body);
}

void PacketHandlers::displayQuestArrow(PacketHandlers&, World& world, BinaryReader& r)
{
    const bool display = (r.readU8() != 0);
    const uint16_t x = r.readU16BE();
    const uint16_t y = r.readU16BE();
    Serial serial = 0;
    if (world.clientVersion >= versions::CV_7090)
        serial = r.readU32BE();
    world.listener().onQuestArrow(serial, display, x, y);
}

// ---------------------------------------------------------------------------------------------
// Chat (0xB2) and feature flags
// ---------------------------------------------------------------------------------------------

void PacketHandlers::chatMessage(PacketHandlers&, World& world, BinaryReader& r)
{
    ChatEvent ev;
    ev.command = r.readU16BE();
    auto& chat = world.chat;

    switch (ev.command)
    {
    case 0x03E8: // create conference
        r.skip(4);
        ev.channel = detail::unicodeBE(r, -1);
        ev.hasPassword = r.readU16BE() == 0x31;
        chat.currentChannel = ev.channel;
        chat.channels.emplace_back(ev.channel, ev.hasPassword);
        break;

    case 0x03E9: // destroy conference
        r.skip(4);
        ev.channel = detail::unicodeBE(r, -1);
        std::erase_if(chat.channels, [&](const auto& c) { return c.first == ev.channel; });
        break;

    case 0x03EB: // ask for a chat name
        chat.status = ChatState::Status::EnabledUserRequest;
        break;

    case 0x03EC: // close chat
        chat = ChatState{};
        break;

    case 0x03ED: // name accepted
        r.skip(4);
        ev.user = detail::unicodeBE(r, -1);
        chat.status = ChatState::Status::Enabled;
        world.requests().chatJoin("General");
        break;

    case 0x03EE: // add user
        r.skip(4);
        r.readU16BE(); // user type
        ev.user = detail::unicodeBE(r, -1);
        break;

    case 0x03EF: // remove user
        r.skip(4);
        ev.user = detail::unicodeBE(r, -1);
        break;

    case 0x03F1: // joined a conference
        r.skip(4);
        ev.channel = detail::unicodeBE(r, -1);
        chat.currentChannel = ev.channel;
        printSystem(world, "You have joined the '" + ev.channel + "' channel.", 0, MessageType::Regular, 1);
        break;

    case 0x03F4: // left a conference
        r.skip(4);
        ev.channel = detail::unicodeBE(r, -1);
        printSystem(world, "You have left the '" + ev.channel + "' channel.", 0, MessageType::Regular, 1);
        break;

    case 0x0025:
    case 0x0026:
    case 0x0027:
    {
        r.skip(4);
        r.readU16BE(); // message type
        ev.user = detail::unicodeBE(r, -1);
        ev.text = detail::unicodeBE(r, -1);

        // Strip the "{...}" colour prefix the server embeds.
        const size_t open = ev.text.find('{');
        const size_t close = ev.text.find('}');
        if (open != std::string::npos && close != std::string::npos && close + 1 > open)
            ev.text.erase(open, close + 1 - open);

        printSystem(world, ev.user + ": " + ev.text, 0, MessageType::Regular, 1);
        break;
    }

    default:
        // 0x0001..0x0024, 0x0028..0x002C: system messages from chat.enu with one argument.
        if ((ev.command >= 0x0001 && ev.command <= 0x0024) || (ev.command >= 0x0028 && ev.command <= 0x002C))
        {
            r.skip(4);
            ev.text = detail::unicodeBE(r, -1);
        }
        break;
    }

    world.listener().onChat(ev);
}

void PacketHandlers::enableLockedFeatures(PacketHandlers&, World& world, BinaryReader& r)
{
    const uint32_t f = world.clientVersion >= versions::CV_60142 ? r.readU32BE() : r.readU16BE();
    world.lockedFeatures = f;
    world.chat.status = (f & locked_features::T2A) ? ChatState::Status::Enabled : ChatState::Status::Disabled;

    uint8_t bc = 0;
    if (f & locked_features::UOR)
        bc |= body_conv::Anim1 | body_conv::Anim2;
    if (f & locked_features::LBR)
        bc |= body_conv::Anim1;
    if (f & locked_features::AOS)
        bc |= body_conv::Anim2;
    if (f & locked_features::SE)
        bc |= body_conv::Anim3;
    if (f & locked_features::ML)
        bc |= body_conv::Anim4;
    world.bodyConversionFlags = bc;
}

void PacketHandlers::logout(PacketHandlers&, World& world, BinaryReader& r)
{
    world.listener().onLogoutResponse((r.readU8() != 0));
}

// ---------------------------------------------------------------------------------------------
// 0xBF general information
// ---------------------------------------------------------------------------------------------

void PacketHandlers::partyPacket(World& world, BinaryReader& r)
{
    auto& party = world.party;
    const uint8_t code = r.readU8();

    switch (code)
    {
    case 1: // add members (full list)
    case 2: // remove member (full list follows)
    {
        const bool add = code == 1;
        const uint8_t count = r.readU8();

        if (count <= 1)
        {
            party.clear();
            world.listener().onPartyChanged();
            return;
        }

        party.clear();

        Serial toRemove = kInvalidSerial;
        if (!add)
            toRemove = r.readU32BE();

        bool removeAll = !add && world.isPlayer(toRemove);
        int done = 0;

        for (uint8_t i = 0; i < count; ++i)
        {
            const Serial serial = r.readU32BE();
            const bool remove = !add && serial == toRemove;

            if (remove && i == 0)
                removeAll = true;

            if (!remove && !removeAll)
            {
                if (i < PartyState::kSize)
                    party.members[i] = serial;
                ++done;
            }

            if (i == 0 && !remove && !removeAll)
                party.leader = serial;
        }

        if (done <= 1 && !add)
            party.clear();

        world.listener().onPartyChanged();
        break;
    }

    case 3: // private party message
    case 4: // public party message
    {
        const Serial from = r.readU32BE();
        std::string text = detail::unicodeBE(r, -1);
        if (!world.party.contains(from))
            break;

        Message msg;
        msg.serial = 0;
        msg.text = std::move(text);
        if (Mobile* m = world.mobile(from))
            msg.name = m->name.empty() ? std::string("<not seeing>") : m->name;
        msg.type = MessageType::Party;
        msg.font = 3;
        msg.textType = TextType::GuildAlly;
        msg.unicode = true;
        world.addMessage(std::move(msg));
        break;
    }

    case 7: // invitation
        party.inviter = r.readU32BE();
        world.listener().onPartyInvite(party.inviter);
        break;

    default: break;
    }
}

void PacketHandlers::extendedCommand(PacketHandlers& self, World& world, BinaryReader& r)
{
    const uint16_t cmd = r.readU16BE();

    if (auto hook = self._extendedHooks.find(cmd); hook != self._extendedHooks.end() && hook->second)
    {
        hook->second(world, r);
        return;
    }

    Player* player = world.player();

    switch (cmd)
    {
    case 0x01: // fast walk prevention keys: Movement and input (setExtendedHook)
    case 0x02:
        break;

    case 0x04: // close generic gump
    {
        const Serial gump = r.readU32BE();
        const uint32_t button = r.readU32BE();
        world.listener().onCloseServerGump(gump, button);
        break;
    }

    case 0x06:
        partyPacket(world, r);
        break;

    case 0x08: // map change
        world.setMapIndex(r.readU8());
        break;

    case 0x0C: // close status bar
        world.listener().onCloseUi(CloseUiKind::Status, r.readU32BE());
        break;

    case 0x10: // display equip info (pre-AOS item identification)
    {
        Item* item = world.item(r.readU32BE());
        if (!item)
            return;

        const uint32_t cliloc = r.readU32BE();
        if (cliloc > 0)
        {
            std::string str = detail::clilocOr(world, cliloc, {});
            if (!str.empty())
                item->name = str;

            Message msg;
            msg.serial = item->serial;
            msg.text = std::move(str);
            msg.name = item->name;
            msg.hue = 0x3B2;
            msg.font = 3;
            msg.textType = TextType::Object;
            msg.unicode = true;
            msg.cliloc = cliloc;
            world.addMessage(std::move(msg));
        }

        std::string info;
        uint16_t crafterLen = 0;
        uint32_t next = r.readU32BE();

        if (next == 0xFFFFFFFDu)
        {
            crafterLen = r.readU16BE();
            if (crafterLen > 0)
            {
                info += detail::clilocOr(world, 1050043, "crafted by ");
                info += detail::ascii(r, crafterLen);
            }
        }

        if (crafterLen != 0)
            next = r.readU32BE();

        if (next == 0xFFFFFFFCu)
            info += "[Unidentified";

        int count = 0;
        while (r.position() + 4 < r.size())
        {
            if (count != 0 || next == 0xFFFFFFFDu || next == 0xFFFFFFFCu)
                next = r.readU32BE();

            const int16_t charges = r.readI16BE();
            const std::string attr = world.clilocs() ? world.clilocs()->get(next) : "#" + std::to_string(next);

            if (!attr.empty())
            {
                if (charges == -1)
                {
                    info += count > 0 ? "/" : " [";
                    info += attr;
                }
                else
                {
                    info += "\n[" + attr + " : " + std::to_string(charges) + "]";
                    count += 20;
                }
            }
            ++count;
        }

        if ((count < 20 && count > 0) || (next == 0xFFFFFFFCu && count == 0))
            info += ']';

        if (!info.empty())
        {
            Message msg;
            msg.serial = item->serial;
            msg.text = std::move(info);
            msg.name = item->name;
            msg.hue = 0x3B2;
            msg.font = 3;
            msg.textType = TextType::Object;
            msg.unicode = true;
            world.addMessage(std::move(msg));
        }
        break;
    }

    case 0x14: // context menu
    {
        PopupMenu menu;
        const uint16_t mode = r.readU16BE();
        const bool isNewCliloc = mode >= 2;
        menu.serial = r.readU32BE();
        const uint8_t count = r.readU8();

        for (uint8_t i = 0; i < count; ++i)
        {
            PopupMenuEntry e;
            if (isNewCliloc)
            {
                e.cliloc = r.readU32BE();
                e.index = r.readU16BE();
                e.flags = r.readU16BE();
            }
            else
            {
                e.index = r.readU16BE();
                e.cliloc = uint32_t(r.readU16BE()) + 3000000u;
                e.flags = r.readU16BE();
                if (e.flags & 0x84)
                    r.skip(2);
                if (e.flags & 0x40)
                    r.skip(2);
                if (e.flags & 0x20)
                    e.replacedHue = r.readU16BE();
            }
            if (e.flags & 0x01)
                e.hue = 0x0386;
            menu.entries.push_back(e);
        }

        world.listener().onContextMenu(menu);
        break;
    }

    case 0x16: // close user interface windows
    {
        const uint32_t id = r.readU32BE();
        const Serial serial = r.readU32BE();
        world.listener().onCloseUi(CloseUiKind(id), serial);
        break;
    }

    case 0x18: // map patches: the map loader applies them
        world.listener().onMapPatches(r.rest());
        break;

    case 0x19: // extended stats
    {
        const uint8_t version = r.readU8();
        const Serial serial = r.readU32BE();

        auto bondedState = [&]() {
            if (Mobile* bonded = world.mobile(serial))
            {
                bonded->setDead((r.readU8() != 0));
                world.listener().onEntityUpdated(*bonded);
            }
        };
        auto statLocks = [&]() {
            if (player && serial == player->serial)
            {
                r.readU8(); // update gump
                const uint8_t state = r.readU8();
                player->strLock = SkillLock((state >> 4) & 3);
                player->dexLock = SkillLock((state >> 2) & 3);
                player->intLock = SkillLock(state & 3);
                world.listener().onStatsChanged(*player);
            }
        };

        switch (version)
        {
        case 0: bondedState(); break;
        case 2: statLocks(); break;
        case 5:
        {
            const size_t pos = r.position();
            r.readU8(); // zero
            const uint8_t type2 = r.readU8();

            if (type2 == 0xFF)
            {
                const uint8_t status = r.readU8();
                const uint16_t animation = r.readU16BE();
                const uint16_t frame = r.readU16BE();

                if (status == 0 && animation == 0 && frame == 0)
                {
                    r.seek(pos);
                    bondedState();
                }
                else if (Mobile* m = world.mobile(serial))
                {
                    auto& a = m->animation;
                    a.source = ServerAnimation::Source::FrameSet;
                    a.action = animation;
                    a.frameCount = frame;
                    ++a.sequence;
                    world.listener().onAnimationRequested(*m);
                }
            }
            else if (player && serial == player->serial)
            {
                r.seek(pos);
                statLocks();
            }
            break;
        }
        default: break;
        }
        break;
    }

    case 0x1B: // new spellbook content
    {
        r.skip(2);
        const Serial serial = r.readU32BE();
        Item& book = world.getOrCreateItem(serial);
        book.graphic = r.readU16BE();

        SpellbookContent content;
        content.graphic = book.graphic;
        content.type = r.readU16BE();
        for (int j = 0; j < 2; ++j)
        {
            uint32_t spells = 0;
            for (int i = 0; i < 4; ++i)
                spells |= uint32_t(r.readU8()) << (i * 8);
            content.spells |= uint64_t(spells) << (32 * j);
        }
        world.spellbooks[serial] = content;
        world.listener().onSpellbookContentsChanged(book);
        break;
    }

    case 0x1D: // custom house revision
    {
        const Serial serial = r.readU32BE();
        const uint32_t revision = r.readU32BE();
        auto it = world.customHouseRevisions.find(serial);

        if (!world.item(serial) || it == world.customHouseRevisions.end() || it->second != revision)
            world.addCustomHouseRequest(serial);
        else
            world.listener().onHouseRevision(serial, revision);
        break;
    }

    case 0x20: // house customization
    {
        const Serial serial = r.readU32BE();
        const uint8_t type = r.readU8();
        r.readU16BE(); // graphic
        r.readU16BE(); // x
        r.readU16BE(); // y
        r.readI8();    // z
        if (type == 4 || type == 5)
            world.listener().onHouseCustomization(serial, type);
        break;
    }

    case 0x21: // clear weapon ability
        if (player)
        {
            for (auto& a : player->abilities)
                a &= 0x7F;
            world.listener().onStatsChanged(*player);
        }
        break;

    case 0x22: // damage
    {
        r.skip(1);
        const Serial serial = r.readU32BE();
        if (world.get(serial))
        {
            const uint8_t amount = r.readU8();
            if (amount > 0)
                world.listener().onDamage(serial, amount);
        }
        break;
    }

    case 0x25: // spell icon state
    {
        const uint16_t spell = r.readU16BE();
        const bool active = (r.readU8() != 0);
        if (active)
            world.activeSpellIcons.insert(spell);
        else
            world.activeSpellIcons.erase(spell);
        world.listener().onSpellIconState(spell, active);
        break;
    }

    case 0x26: // movement speed mode
    {
        uint8_t val = r.readU8();
        if (val > uint8_t(CharacterSpeed::FastUnmountAndCantRun))
            val = 0;
        if (player)
            player->speedMode = CharacterSpeed(val);
        break;
    }

    case 0x2A: // race change window
    {
        const bool female = (r.readU8() != 0);
        world.listener().onRaceChangeWindow(female, r.readU8());
        break;
    }

    case 0x2B: // set animation frame by low 16 bits of the serial
    {
        const uint16_t low = r.readU16BE();
        const uint8_t animId = r.readU8();
        const uint8_t frame = r.readU8();
        Mobile* found = nullptr;
        world.forEachMobile([&](Mobile& m) {
            if (!found && (m.serial & 0xFFFF) == low)
                found = &m;
        });
        if (found)
        {
            auto& a = found->animation;
            a.source = ServerAnimation::Source::FrameSet;
            a.action = animId;
            a.frameCount = frame;
            ++a.sequence;
            world.listener().onAnimationRequested(*found);
        }
        break;
    }

    default: break; // 0x00, 0x11, 0xBEEF and unknown sub-commands are ignored
    }
}

// ---------------------------------------------------------------------------------------------
// AOS+ packets (tooltips, custom houses, buffs). Unused by the T2A ruleset, kept for parity.
// ---------------------------------------------------------------------------------------------

void PacketHandlers::oplInfo(PacketHandlers&, World& world, BinaryReader& r)
{
    if (!world.tooltipsEnabled)
        return;

    const Serial serial = r.readU32BE();
    const uint32_t revision = r.readU32BE();
    if (!world.isRevisionEqual(serial, revision))
        world.addMegaClilocRequest(serial);
}

void PacketHandlers::megaCliloc(PacketHandlers&, World& world, BinaryReader& r)
{
    if (!world.inGame())
        return;

    if (r.readU16BE() > 1)
        return;

    const Serial serial = r.readU32BE();
    r.skip(2);
    const uint32_t revision = r.readU32BE();

    Entity* entity = world.mobile(serial);
    if (!entity)
        entity = world.item(serial);

    struct Line
    {
        uint32_t cliloc;
        std::string text;
        uint32_t argCliloc;
    };
    std::vector<Line> lines;

    while (r.position() < r.size())
    {
        const uint32_t cliloc = r.readU32BE();
        if (cliloc == 0)
            break;

        const uint16_t length = r.readU16BE();
        std::string argument = length ? detail::unicodeLE(r, length / 2) : std::string{};

        auto str = detail::translate(world, cliloc, argument, true);
        if (!str)
            continue;

        uint32_t argCliloc = 0;
        {
            // "#1234" arguments name a cliloc; remember it as the name cliloc.
            std::vector<std::string_view> parts;
            std::string_view a = argument;
            size_t start = 0;
            while (start <= a.size())
            {
                size_t end = a.find('#', start);
                if (end == std::string_view::npos)
                    end = a.size();
                if (end > start)
                    parts.push_back(a.substr(start, end - start));
                start = end + 1;
            }
            if (parts.size() == 2)
                parseUInt(parts[1], argCliloc);
        }

        switch (cliloc)
        {
        case 1080418:
            if (world.clientVersion >= versions::CV_60143)
                *str = "<basefont color=#40a4fe>" + *str + "</basefont>";
            break;
        case 1061170:
        {
            uint32_t strength = 0;
            if (parseUInt(argument, strength) && world.player() && world.player()->strength < strength)
                *str = "<basefont color=#FF0000>" + *str + "</basefont>";
            break;
        }
        case 1062613: *str = "<basefont color=#FFCC33>" + *str + "</basefont>"; break;
        case 1159561: *str = "<basefont color=#b66dff>" + *str + "</basefont>"; break;
        default: break;
        }

        for (auto it = lines.begin(); it != lines.end(); ++it)
        {
            if (it->cliloc == cliloc && it->text == *str)
            {
                lines.erase(it);
                break;
            }
        }
        lines.push_back({cliloc, std::move(*str), argCliloc});
    }

    ObjectProperty prop;
    prop.revision = revision;
    for (size_t i = 0; i < lines.size(); ++i)
    {
        prop.clilocs.push_back(lines[i].cliloc);
        if (i == 0)
        {
            prop.name = lines[i].text;
            if (entity && !isMobileSerial(serial))
            {
                entity->name = lines[i].text;
                prop.nameCliloc = lines[i].argCliloc > 0 ? lines[i].argCliloc : lines[i].cliloc;
            }
        }
        else
        {
            if (!prop.data.empty())
                prop.data += '\n';
            prop.data += lines[i].text;
        }
    }

    world.setProperties(serial, std::move(prop));
    if (entity)
        world.listener().onNameChanged(*entity);
}

void PacketHandlers::customHouse(PacketHandlers&, World& world, BinaryReader& r)
{
    r.readU8(); // compression type (0x03)
    r.skip(1); // enable response
    const Serial serial = r.readU32BE();
    Item* foundation = world.item(serial);
    const uint32_t revision = r.readU32BE();

    if (!foundation || !foundation->isMulti || !world.multiBounds)
        return;

    const auto bounds = world.multiBounds(foundation->graphic);
    if (!bounds || (bounds->minX == 0 && bounds->minY == 0 && bounds->maxY == 0 && bounds->maxX == 0))
        return; // missing multi data

    r.skip(4);

    CustomHouseData house;
    house.serial = serial;
    house.revision = revision;

    const int16_t minX = bounds->minX;
    const int16_t minY = bounds->minY;
    const int16_t maxY = bounds->maxY;

    const uint8_t planes = r.readU8();
    std::vector<uint8_t> buffer;

    for (uint8_t plane = 0; plane < planes; ++plane)
    {
        const uint32_t header = r.readU32BE();
        const size_t dlen = ((header & 0xFF0000) >> 16) | ((header & 0xF0) << 4);
        const size_t clen = ((header & 0xFF00) >> 8) | ((header & 0x0F) << 8);
        const int planeZ = int((header & 0x0F000000) >> 24);
        const int planeMode = int((header & 0xF0000000) >> 28);

        if (clen == 0)
            continue;

        auto compressed = r.rest().first(std::min(clen, r.remaining()));
        r.skip(clen);

        buffer.clear();
        if (!io::inflate(compressed, buffer, dlen))
            continue;
        buffer.resize(dlen); // planes are read by their declared size

        BinaryReader d(buffer);
        const auto planeHeight = int8_t(planeZ > 0 ? ((planeZ - 1) % 4) * 20 + 7 : 0);

        switch (planeMode)
        {
        case 0:
            for (size_t i = 0, c = dlen / 5; i < c; ++i)
            {
                const uint16_t id = d.readU16BE();
                const int8_t x = d.readI8();
                const int8_t y = d.readI8();
                const int8_t z = d.readI8();
                if (id != 0)
                    house.components.push_back({id, x, y, z});
            }
            break;

        case 1:
            for (size_t i = 0, c = dlen >> 2; i < c; ++i)
            {
                const uint16_t id = d.readU16BE();
                const int8_t x = d.readI8();
                const int8_t y = d.readI8();
                if (id != 0)
                    house.components.push_back({id, x, y, planeHeight});
            }
            break;

        case 2:
        {
            int16_t offX, offY, multiHeight;
            if (planeZ <= 0)
            {
                offX = minX;
                offY = minY;
                multiHeight = int16_t(maxY - minY + 2);
            }
            else if (planeZ <= 4)
            {
                offX = int16_t(minX + 1);
                offY = int16_t(minY + 1);
                multiHeight = int16_t(maxY - minY);
            }
            else
            {
                offX = minX;
                offY = minY;
                multiHeight = int16_t(maxY - minY + 1);
            }
            if (multiHeight <= 0)
                break;

            for (size_t i = 0, c = dlen >> 1; i < c; ++i)
            {
                const uint16_t id = d.readU16BE();
                const auto x = int8_t(int(i) / multiHeight + offX);
                const auto y = int8_t(int(i) % multiHeight + offY);
                if (id != 0)
                    house.components.push_back({id, x, y, planeHeight});
            }
            break;
        }
        default: break;
        }
    }

    world.customHouseRevisions[serial] = revision;
    world.listener().onCustomHouse(house);
}

void PacketHandlers::buffDebuff(PacketHandlers&, World& world, BinaryReader& r)
{
    Player* player = world.player();
    if (!player)
        return;

    constexpr uint16_t kIconStart = 0x03E9;
    constexpr uint16_t kIconStartNew = 0x0466;

    r.readU32BE(); // serial
    const uint16_t type = r.readU16BE();
    const auto iconId =
        uint16_t(type >= kIconStartNew ? type - (kIconStartNew - 125) : uint16_t(type - kIconStart));

    if (iconId >= world.buffTable.size())
        return;

    const uint16_t count = r.readU16BE();
    if (count == 0)
    {
        player->buffs.erase(type);
        world.listener().onBuffsChanged();
        return;
    }

    for (uint16_t i = 0; i < count; ++i)
    {
        BuffIcon b;
        b.type = type;
        b.iconId = iconId;
        r.readU16BE(); // source type
        r.skip(2);
        r.readU16BE(); // icon
        r.readU16BE(); // queue index
        r.skip(4);
        b.timerSeconds = r.readU16BE();
        r.skip(3);
        b.titleCliloc = r.readU32BE();
        b.descriptionCliloc = r.readU32BE();
        b.extraCliloc = r.readU32BE();

        r.readU16BE(); // arg length
        std::string args = detail::unicodeLE(r, 2);
        args += detail::unicodeLE(r, -1);
        b.titleArgs = args;

        r.readU16BE();
        b.descriptionArgs = detail::unicodeLE(r, -1);
        r.readU16BE();
        b.extraArgs = detail::unicodeLE(r, -1);

        if (world.clilocs())
        {
            std::string title = detail::translate(world, b.titleCliloc, args, true).value_or("");
            std::string description;
            if (b.descriptionCliloc != 0)
            {
                description = "\n" + detail::translate(world, b.descriptionCliloc,
                                                       b.descriptionArgs.empty() ? args : b.descriptionArgs, true)
                                         .value_or("");
                if (description.size() < 2)
                    description.clear();
            }
            std::string extra;
            if (b.extraCliloc != 0)
            {
                extra = detail::translate(world, b.extraCliloc, b.extraArgs.empty() ? args : b.extraArgs, true)
                            .value_or("");
                if (extra.find_first_not_of(" \t\r\n") != std::string::npos)
                    extra = "\n" + extra;
                else
                    extra.clear();
            }
            b.text = "<left>" + title + description + extra + "</left>";
        }

        player->buffs[type] = std::move(b);
    }

    world.listener().onBuffsChanged();
}

void PacketHandlers::displayWaypoint(PacketHandlers&, World& world, BinaryReader& r)
{
    Waypoint w;
    w.serial = r.readU32BE();
    w.x = r.readU16BE();
    w.y = r.readU16BE();
    w.z = r.readI8();
    w.map = r.readU8();
    w.type = r.readU16BE();
    w.ignoreObject = r.readU16BE() != 0;
    w.cliloc = r.readU32BE();
    w.name = detail::unicodeLE(r, -1);
    world.listener().onWaypoint(w);
}

void PacketHandlers::removeWaypoint(PacketHandlers&, World& world, BinaryReader& r)
{
    world.listener().onWaypointRemoved(r.readU32BE());
}

void PacketHandlers::krriosClientSpecial(PacketHandlers&, World& world, BinaryReader& r)
{
    const uint8_t type = r.readU8();

    switch (type)
    {
    case 0x01: // custom party info
    case 0x02: // guild track info
    {
        const bool locations = type == 0x01 || (r.readU8() != 0);
        std::vector<WorldMapEntityUpdate> updates;
        Serial serial;
        while (r.remaining() >= 4 && (serial = r.readU32BE()) != 0)
        {
            if (!locations)
                continue;
            WorldMapEntityUpdate u;
            u.serial = serial;
            u.x = r.readU16BE();
            u.y = r.readU16BE();
            u.map = r.readU8();
            u.hits = type == 1 ? 0 : r.readU8();
            u.isGuild = type == 0x02;
            updates.push_back(u);
        }
        world.listener().onWorldMapEntities(updates);
        break;
    }
    case 0xFE: world.requests().razorAck(); break;
    default: break; // 0x00 ack, 0x03 runebook, 0x04 guardlines, 0xF0: not used
    }
}

} // namespace uo::world
