// SPDX-License-Identifier: MIT
#include "WorldScene.h"

#include "GameClient.h"
#include "LoginScene.h"

#include "uo/net/OutgoingPackets.h"

#include <algorithm>
#include <cstdlib>

using namespace ax;
using uo::world::Direction;
using KeyCode = KeyboardEvent::KeyCode;

namespace
{
constexpr const char* kFont = "fonts/arial.ttf";
constexpr float kWalkDelay  = 0.4f;  // on-foot walking pace, seconds per tile
constexpr float kRunDelay   = 0.2f;

// The map the world is on; the world reports -1 until 0x1B / 0xBF 0x08 set it.
int currentMap()
{
    return std::max(0, GameClient::instance().world().mapIndex);
}

Color32 notorietyColor(uo::world::Notoriety notoriety)
{
    const auto n = static_cast<std::uint8_t>(notoriety);
    switch (n)
    {
    case 1: return Color32(80, 140, 255, 255);   // innocent
    case 2: return Color32(60, 220, 60, 255);    // ally
    case 3:                                        // attackable
    case 4: return Color32(170, 170, 170, 255);  // criminal
    case 5: return Color32(240, 150, 40, 255);   // enemy
    case 6: return Color32(230, 40, 40, 255);    // murderer
    case 7: return Color32(240, 240, 80, 255);   // invulnerable
    default: return Color32(200, 200, 200, 255);
    }
}
}  // namespace

Vec2 WorldScene::tileToWorld(int x, int y, int z)
{
    // ClassicUO: screen = ((x - y) * 22, (x + y) * 22 - z * 4), y growing down.
    return Vec2(static_cast<float>((x - y) * 22), -static_cast<float>((x + y) * 22 - z * 4));
}

int WorldScene::depth(int x, int y, int z, int bias)
{
    return (x + y) * 1024 + (std::clamp(z, -128, 127) + 128) * 2 + bias;
}

bool WorldScene::init()
{
    if (!Scene::init())
        return false;

    addChild(LayerColor::create(Color32(0, 0, 0, 255)), -10);

    _worldNode = Node::create();
    addChild(_worldNode);

    auto size   = _director->getVisibleSize();
    auto origin = _director->getVisibleOrigin();

    _journal = Label::createWithTTF("", kFont, 16);
    _journal->setAnchorPoint(Vec2(0, 0));
    _journal->setPosition(origin + Vec2(12, 12));
    _journal->setDimensions(size.width * 0.6f, 0);
    _journal->enableOutline(Color32::black, 1);
    addChild(_journal, 10);

    _coords = Label::createWithTTF("", kFont, 14);
    _coords->setAnchorPoint(Vec2(1, 1));
    _coords->setPosition(origin + Vec2(size.width - 12, size.height - 12));
    _coords->enableOutline(Color32::black, 1);
    addChild(_coords, 10);

    _keys                = KeyboardEventListener::create();
    _keys->onKeyPressed  = AX_CALLBACK_1(WorldScene::onKeyPressed, this);
    _keys->onKeyReleased = AX_CALLBACK_1(WorldScene::onKeyReleased, this);
    _eventDispatcher->addEventListenerWithSceneGraphPriority(_keys, this);

    scheduleUpdate();
    return true;
}

void WorldScene::onEnter()
{
    Scene::onEnter();
    auto& gc    = GameClient::instance();
    auto& world = gc.world();

    gc.entityUpdatedHandler = [this](const uo::world::Entity& e) { syncEntity(e); };
    gc.entityRemovedHandler = [this](uo::world::Serial s) { removeEntity(s); };
    gc.messageHandler       = [this](const uo::world::Message&) { refreshJournal(); };
    gc.disconnectedHandler  = [this] { _director->replaceScene(utils::createInstance<LoginScene>()); };

    world.forEachMobile([this](uo::world::Mobile& m) { syncEntity(m); });
    world.forEachItem([this](uo::world::Item& i) { syncEntity(i); });
    refreshJournal();
    streamBlocks();
    centerCamera();

    // Smoke-test hook: AXMOLUO_SCREENSHOT=/path/shot.png captures the world view and quits.
    if (const char* shot = std::getenv("AXMOLUO_SCREENSHOT"))
    {
        std::string path = shot;
        scheduleOnce(
            [this, path](float) {
                utils::captureScreen([this](bool ok, std::string_view file) {
                    if (_quitting)
                        return;
                    _quitting = true;
                    AXLOGI("AxmolUO screenshot {}: {}", ok ? "saved" : "failed", file);
                    _director->end();
                }, path);
            },
            2.0f, "screenshot");
    }
}

void WorldScene::onExit()
{
    auto& gc                = GameClient::instance();
    gc.entityUpdatedHandler = nullptr;
    gc.entityRemovedHandler = nullptr;
    gc.messageHandler       = nullptr;
    gc.disconnectedHandler  = nullptr;
    Scene::onExit();
}

void WorldScene::update(float dt)
{
    GameClient::instance().update(dt);
    handleWalking(dt);

    if (const auto* p = GameClient::instance().world().player())
    {
        if (p->x != _lastPlayerX || p->y != _lastPlayerY)
        {
            _lastPlayerX = p->x;
            _lastPlayerY = p->y;
            streamBlocks();
            _coords->setString(fmt::format("{}, {}, {}", p->x, p->y, p->z));
        }
        centerCamera();
    }
}

void WorldScene::centerCamera()
{
    const auto* p = GameClient::instance().world().player();
    if (!p)
        return;
    auto size   = _director->getVisibleSize();
    auto origin = _director->getVisibleOrigin();
    _worldNode->setPosition(origin + Vec2(size.width / 2, size.height / 2) - tileToWorld(p->x, p->y, p->z));
}

void WorldScene::streamBlocks()
{
    auto& gc      = GameClient::instance();
    const auto* p = gc.world().player();
    const auto* map = gc.install().map(currentMap());
    if (!p || !map)
        return;

    int pbx = p->x >> 3, pby = p->y >> 3;
    std::set<BlockKey> wanted;
    for (int bx = pbx - kViewBlocks; bx <= pbx + kViewBlocks; ++bx)
        for (int by = pby - kViewBlocks; by <= pby + kViewBlocks; ++by)
            if (bx >= 0 && by >= 0 && bx < map->blockWidth() && by < map->blockHeight())
                wanted.insert({bx, by});

    for (auto it = _blocks.begin(); it != _blocks.end();)
    {
        if (!wanted.count(it->first))
        {
            for (Node* n : it->second)
                n->removeFromParent();
            it = _blocks.erase(it);
        }
        else
        {
            ++it;
        }
    }

    for (const auto& key : wanted)
        if (!_blocks.count(key))
            buildBlock(key);
}

void WorldScene::buildBlock(BlockKey key)
{
    auto& gc        = GameClient::instance();
    const auto* map = gc.install().map(currentMap());
    auto& textures  = gc.textures();
    auto& nodes     = _blocks[key];

    auto land = map->landBlock(key.bx, key.by);
    for (int cy = 0; cy < 8; ++cy)
    {
        for (int cx = 0; cx < 8; ++cx)
        {
            const auto& cell = land[cy * 8 + cx];
            // 0x0002 is the "no draw" void tile; ClassicUO skips it too.
            if (cell.tileId == 0x0002)
                continue;
            Texture2D* tex = textures.land(cell.tileId);
            if (!tex)
                continue;
            int x = key.bx * 8 + cx, y = key.by * 8 + cy;
            auto* s = Sprite::createWithTexture(tex);
            s->setAnchorPoint(Vec2(0.5f, 0.5f));
            s->setPosition(tileToWorld(x, y, cell.z));
            _worldNode->addChild(s, depth(x, y, cell.z, 0));
            nodes.push_back(s);
        }
    }

    for (const auto& st : map->staticsBlock(key.bx, key.by))
    {
        Texture2D* tex = textures.statik(st.graphic);
        if (!tex)
            continue;
        int x = key.bx * 8 + st.x, y = key.by * 8 + st.y;
        auto* s = Sprite::createWithTexture(tex);
        // Statics stand on the bottom vertex of their tile's diamond.
        s->setAnchorPoint(Vec2(0.5f, 0));
        s->setPosition(tileToWorld(x, y, st.z) - Vec2(0, 22));
        _worldNode->addChild(s, depth(x, y, st.z, 1));
        nodes.push_back(s);
    }
}

void WorldScene::syncEntity(const uo::world::Entity& e)
{
    auto& gc = GameClient::instance();
    Node* node = nullptr;

    // Only what stands in the world is drawn; equipment and container contents are not.
    const auto* item = e.isItem() ? static_cast<const uo::world::Item*>(&e) : nullptr;
    if (item && !item->onGround())
    {
        removeEntity(e.serial);
        return;
    }
    const auto* mobile = e.isMobile() ? static_cast<const uo::world::Mobile*>(&e) : nullptr;

    if (auto it = _entities.find(e.serial); it != _entities.end())
    {
        node = it->second;
    }
    else
    {
        if (e.isMobile())
        {
            // Placeholder until mobile animations are ported: a notoriety-coloured marker and name.
            node     = Node::create();
            auto dot = DrawNode::create();
            dot->drawSolidCircle(Vec2(0, 30), 10, 0, 20, Color(notorietyColor(mobile->notoriety)));
            dot->setName("marker");
            node->addChild(dot);
            auto name = Label::createWithTTF(e.name, kFont, 13);
            name->setPosition(Vec2(0, 58));
            name->enableOutline(Color32::black, 1);
            name->setName("name");
            node->addChild(name);
        }
        else
        {
            Texture2D* tex = gc.textures().statik(e.graphic);
            if (!tex)
                return;
            auto* s = Sprite::createWithTexture(tex);
            s->setAnchorPoint(Vec2(0.5f, 0));
            node = s;
        }
        _worldNode->addChild(node);
        _entities[e.serial] = node;
    }

    if (mobile)
    {
        if (auto* name = dynamic_cast<Label*>(node->getChildByName("name")))
        {
            name->setString(e.name);
            name->setTextColor(notorietyColor(mobile->notoriety));
        }
        node->setPosition(tileToWorld(e.x, e.y, e.z));
    }
    else
    {
        node->setPosition(tileToWorld(e.x, e.y, e.z) - Vec2(0, 22));
    }
    node->setLocalZOrder(depth(e.x, e.y, e.z, 2));
}

void WorldScene::removeEntity(std::uint32_t serial)
{
    if (auto it = _entities.find(serial); it != _entities.end())
    {
        it->second->removeFromParent();
        _entities.erase(it);
    }
}

void WorldScene::handleWalking(float dt)
{
    _walkCooldown = std::max(0.f, _walkCooldown - dt);
    if (_walkCooldown > 0 || _held.empty())
        return;

    bool up    = _held.count(KeyCode::KEY_UP_ARROW) || _held.count(KeyCode::KEY_KP_UP);
    bool down  = _held.count(KeyCode::KEY_DOWN_ARROW) || _held.count(KeyCode::KEY_KP_DOWN);
    bool left  = _held.count(KeyCode::KEY_LEFT_ARROW) || _held.count(KeyCode::KEY_KP_LEFT);
    bool right = _held.count(KeyCode::KEY_RIGHT_ARROW) || _held.count(KeyCode::KEY_KP_RIGHT);

    // Screen directions to UO's (rotated 45 degrees): screen up is north-west ("Up").
    int dx = (right ? 1 : 0) - (left ? 1 : 0);
    int dy = (up ? 1 : 0) - (down ? 1 : 0);
    if (dx == 0 && dy == 0)
        return;

    static constexpr Direction table[3][3] = {
        // dx = -1           0                +1
        {Direction::South, Direction::Down, Direction::East},  // dy = -1 (screen down)
        {Direction::Left, Direction::North, Direction::Right},  // dy = 0 (centre unused)
        {Direction::West, Direction::Up, Direction::North},     // dy = +1 (screen up)
    };
    Direction dir = table[dy + 1][dx + 1];

    // The walker (uo::movement) predicts the step and sends 0x02.
    auto& gc = GameClient::instance();
    if (gc.walkHandler && gc.walkHandler(dir, _running))
        _walkCooldown = _running ? kRunDelay : kWalkDelay;
}

std::string WorldScene::journalText(const uo::world::Message& j) const
{
    // Cliloc messages arrive translated when GameClient's resolver had the table; otherwise
    // they carry the number and arguments.
    std::string text = j.text;
    if (text.empty() && j.cliloc)
    {
        text = GameClient::instance().install().cliloc().format(static_cast<std::int32_t>(j.cliloc), j.clilocArgs);
        if (!j.affix.empty())
            text = j.affixPrepend ? j.affix + text : text + j.affix;
    }
    if (!j.name.empty() && j.serial != 0xFFFFFFFF && j.name != "System")
        return j.name + ": " + text;
    return text;
}

void WorldScene::refreshJournal()
{
    const auto& journal = GameClient::instance().world().journal();
    std::string out;
    std::size_t start = journal.size() > 8 ? journal.size() - 8 : 0;
    for (std::size_t i = start; i < journal.size(); ++i)
    {
        out += journalText(journal[i]);
        out += '\n';
    }
    _journal->setString(out);
}

void WorldScene::onKeyPressed(KeyboardEvent* ev)
{
    auto code = ev->getKeyCode();
    if (code == KeyCode::KEY_LEFT_SHIFT || code == KeyCode::KEY_RIGHT_SHIFT)
        _running = true;
    else if (code == KeyCode::KEY_ESCAPE)
    {
        GameClient::instance().session().stop();
        _director->replaceScene(utils::createInstance<LoginScene>());
    }
    else
        _held.insert(code);
}

void WorldScene::onKeyReleased(KeyboardEvent* ev)
{
    auto code = ev->getKeyCode();
    if (code == KeyCode::KEY_LEFT_SHIFT || code == KeyCode::KEY_RIGHT_SHIFT)
        _running = false;
    else
        _held.erase(code);
}
