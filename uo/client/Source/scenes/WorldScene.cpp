// SPDX-License-Identifier: MIT
#include "WorldScene.h"

#include "GameClient.h"
#include "axmol/JournalView.h"
#include "axmol/OverheadTextLayer.h"
#include "axmol/TextSystem.h"
#include "InputRouter.h"
#include "LoginScene.h"

#include "anim/AnimationTextures.h"
#include "world/axmol/HueShader.h"
#include "world/axmol/WorldRenderer.h"

#include "uo/anim/AnimationsLoader.h"
#include "uo/anim/WorldMobiles.h"
#include "uo/net/OutgoingPackets.h"
#include "uo/render/HueTexture.h"
#include "uo/text/JournalText.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

using namespace ax;
using uo::client::input::InputRouter;
using uo::client::input::MouseButton;

namespace
{
constexpr const char* kFont = "fonts/arial.ttf";
// Overhead text starts above the placeholder mobile's name label; with animated mobiles this
// becomes the frame height, as in ClassicUO's Mobile.UpdateTextCoordsV.
constexpr float kMobileTextHeight = 72.f;

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

namespace
{
// Serves uocore art to the renderer from a cache the scene owns, so textures outlive neither
// the scene nor GameClient::shutdown (which drops the shared cache before the last frame).
// Texmaps are cached here since UOTextures has no texmap kind yet.
class SceneTextures final : public uo::render::ITextureSource
{
public:
    SceneTextures(const uo::assets::Installation& install, const uo::assets::Texmaps* texmaps,
                  uo::client::AnimationTextures* animations)
        : _art(install), _texmaps(texmaps), _animations(animations)
    {}

    ~SceneTextures() override
    {
        for (auto& [id, tex] : _texmapCache)
            AX_SAFE_RELEASE(tex);
    }

    bool landArt(std::uint16_t graphic, uo::render::TextureRegion& out) override
    {
        return fill(_art.land(graphic), out);
    }

    bool texmap(std::uint16_t texId, uo::render::TextureRegion& out) override
    {
        if (!_texmaps)
            return false;
        auto it = _texmapCache.find(texId);
        if (it == _texmapCache.end())
            it = _texmapCache.emplace(texId, UOTextures::fromImage(_texmaps->texmap(texId))).first;
        return fill(it->second, out);
    }

    bool itemArt(std::uint16_t graphic, uo::render::TextureRegion& out) override
    {
        return fill(_art.statik(graphic), out);
    }

    bool animFrame(const uo::anim::Frame& frame, uo::render::TextureRegion& out) override
    {
        return _animations && _animations->region(frame, out);
    }

private:
    static bool fill(Texture2D* tex, uo::render::TextureRegion& out)
    {
        if (!tex)
            return false;
        const int w = tex->getWidth(), h = tex->getHeight();
        out = {tex, w, h, 0, 0, w, h};
        return true;
    }

    UOTextures _art;
    const uo::assets::Texmaps* _texmaps;
    uo::client::AnimationTextures* _animations;
    std::unordered_map<std::uint16_t, Texture2D*> _texmapCache;
};
}  // namespace

WorldScene::WorldScene()  = default;
WorldScene::~WorldScene() = default;

int WorldScene::depth(int x, int y, int z, int bias)
{
    return (x + y) * 1024 + (std::clamp(z, -128, 127) + 128) * 2 + bias;
}

bool WorldScene::init()
{
    if (!Scene::init())
        return false;

    addChild(LayerColor::create(Color32(0, 0, 0, 255)), -10);

    auto size   = _director->getVisibleSize();
    auto origin = _director->getVisibleOrigin();

    auto& gc   = GameClient::instance();
    auto& hues = uo::render::HueShader::instance();
    if (!hues.ready())
        hues.init(uo::render::packHueTexture(gc.install().hues().buildHueTexture()));

    // texmaps.mul is optional: without it land is drawn flat.
    auto texmapFile = std::make_unique<uo::io::MulFile>(gc.install().path("texmaps.mul"), gc.install().path("texidx.mul"));
    if (texmapFile->load())
        _texmaps = std::make_unique<uo::assets::Texmaps>(std::move(texmapFile));

    if (const auto* map = gc.install().map(currentMap()))
    {
        _source        = std::make_unique<uo::render::AssetsWorldSource>(*map, gc.install().tileData(), _texmaps.get());
        _map           = std::make_unique<uo::render::WorldMap>(*_source, *_source);
        // Mobile, mount and equipment animations (anim*.mul / AnimationFrame*.uop).
        uo::anim::AnimationsConfig animConfig;
        animConfig.directory         = gc.install().options().directory;
        animConfig.version           = gc.world().clientVersion;
        animConfig.isUopInstallation = gc.install().isUop();
        animConfig.resolve           = [&install = gc.install()](const std::string& name) { return install.path(name); };
        _animLoader = std::make_unique<uo::anim::AnimationsLoader>();
        if (_animLoader->load(animConfig))
        {
            _animCache    = std::make_unique<uo::anim::AnimationCache>(*_animLoader);
            _animTextures = std::make_unique<uo::client::AnimationTextures>(*_animCache);
            const auto& tiles = gc.install().tileData();
            _animator = std::make_unique<uo::anim::WorldMobileAnimator>(
                *_animCache, gc.world(),
                [&tiles](std::uint16_t graphic) {
                    uo::anim::WorldMobileAnimator::ItemInfo info;
                    if (const auto* t = tiles.staticTile(graphic))
                    {
                        info.animId     = t->animId;
                        info.partialHue = (t->flags & uo::assets::TF_PartialHue) != 0;
                        info.isLight    = (t->flags & uo::assets::TF_LightSource) != 0;
                    }
                    return info;
                },
                [&movement = gc.movement()](std::uint32_t serial) { return movement.motion(serial); });
        }
        else
        {
            AXLOGW("AxmolUO: no animation files, mobiles are not drawn");
        }

        _textureSource = std::make_unique<SceneTextures>(gc.install(), _texmaps.get(), _animTextures.get());
        _hitTest       = std::make_unique<uo::render::ArtHitTest>(gc.install().art());

        // animdata.mul is optional: without it animated art (water, fires) shows its first frame.
        auto animData = std::make_unique<uo::assets::AnimData>();
        if (animData->load(gc.install().path("animdata.mul")))
        {
            _animData        = std::move(animData);
            _animatedStatics = std::make_unique<uo::render::AnimatedStatics>(*_source, *_animData);
        }
        _renderer      = uo::render::WorldRenderer::create(*_source, *_textureSource);
    }

    if (_renderer)
    {
        _renderer->setContentSize(size);
        _renderer->setPosition(origin);
        _renderer->setDrawList(&_drawList);
        addChild(_renderer);
    }

    // Mobile name labels, above the world.
    _worldNode = Node::create();
    addChild(_worldNode, 1);

    if (uo::client::text::TextSystem::instance().ready())
    {
        // Speech over heads, between the world and the journal. It covers the visible area,
        // which is the game viewport until the client has a resizable one.
        _overhead = uo::client::text::OverheadTextLayer::create();
        _overhead->setViewport(Rect(0, 0, size.width, size.height));
        _overhead->setPosition(origin);
        _overhead->setAnchorProvider([this](std::uint32_t serial) { return overheadAnchor(serial); });
        addChild(_overhead, 5);

        _journalView = uo::client::text::JournalView::create(Size(size.width * 0.6f, 160));
        _journalView->setPosition(origin + Vec2(12, 12));
        addChild(_journalView, 10);
    }
    else
    {
        _journal = Label::createWithTTF("", kFont, 16);
        _journal->setAnchorPoint(Vec2(0, 0));
        _journal->setPosition(origin + Vec2(12, 12));
        _journal->setDimensions(size.width * 0.6f, 0);
        _journal->enableOutline(Color32::black, 1);
        addChild(_journal, 10);
    }

    // Gumps sit above the game view and the journal.
    if (auto* gumps = GameClient::instance().gumps())
        addChild(gumps->createManager(), 20);

    _coords = Label::createWithTTF("", kFont, 14);
    _coords->setAnchorPoint(Vec2(1, 1));
    _coords->setPosition(origin + Vec2(size.width - 12, size.height - 12));
    _coords->enableOutline(Color32::black, 1);
    addChild(_coords, 10);

    // Mouse and arrow-key walking, clicks, double-clicks and Escape. Until gumps claim their own
    // clicks, the whole window is the game world.
    InputRouter::Callbacks callbacks;
    callbacks.isOverWorld   = [](Vec2) { return true; };
    callbacks.onClick       = [this](MouseButton b, Vec2 p) { onClick(b, p); };
    callbacks.onDoubleClick = [this](MouseButton b, Vec2 p) { onDoubleClick(b, p); };
    callbacks.onEscape      = [this] { onEscape(); };
    _input = std::make_unique<InputRouter>(std::move(callbacks), GameClient::instance().movement().options.input);

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
    gc.messageHandler       = [this](const uo::world::Message& m) {
        appendJournal(m);
        showOverhead(m);
    };
    gc.disconnectedHandler  = [this] { _director->replaceScene(utils::createInstance<LoginScene>()); };

    _input->attach(this);
    gc.cancelDoubleClickHandler = [this] { _input->cancelDoubleClick(); };

    world.forEachMobile([this](uo::world::Mobile& m) { syncEntity(m); });
    world.forEachItem([this](uo::world::Item& i) { syncEntity(i); });
    refreshJournal();
    streamBlocks();
    centerCamera();
    rebuildDrawList();

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
    gc.cancelDoubleClickHandler = nullptr;
    _input->detach();
    Scene::onExit();
}

void WorldScene::update(float dt)
{
    auto& gc = GameClient::instance();
    gc.update(dt);
    gc.updateMovement(_input->movementIntent(playerScreenPoint(), gc.movement().autoWalker().active()), dt);

    if (const auto* p = gc.world().player())
    {
        if (p->x != _lastPlayerX || p->y != _lastPlayerY)
        {
            _lastPlayerX = p->x;
            _lastPlayerY = p->y;
            streamBlocks();
            _coords->setString(fmt::format("{}, {}, {}", p->x, p->y, p->z));
        }
        placeMobiles();
        centerCamera();
    }

    // The server's season (0xBC) swaps land and static graphics and hides winter foliage.
    static_assert(static_cast<int>(uo::world::Season::Winter) == static_cast<int>(uo::render::SeasonId::Winter) &&
                  static_cast<int>(uo::world::Season::Desolation) == static_cast<int>(uo::render::SeasonId::Desolation));
    if (_map)
    {
        const auto season = static_cast<uo::render::SeasonId>(gc.world().season);
        if (season != _map->season())
        {
            _map->setSeason(season);
            _drawListDirty = true;
        }
    }

    // Water, fires and other animated art step through their animdata frames.
    if (_animatedStatics && _animatedStatics->update(static_cast<std::uint32_t>(gc.movement().now())))
        _drawListDirty = true;

    // Steps completed this frame move mobiles to new tiles; then their frames advance. The draw
    // list holds frame pointers, so it is rebuilt whenever the animator reports a change.
    syncMobileTiles();
    if (_animator && _animator->update(gc.movement().now()))
        _drawListDirty = true;

    rebuildDrawList();
}

Vec2 WorldScene::mobilePosition(const uo::world::Entity& e) const
{
    Vec2 pos = tileToWorld(e.x, e.y, e.z);
    if (const auto* clock = GameClient::instance().movement().motion(e.serial))
    {
        // ClassicUO draws at (x + Offset.X, y + Offset.Y - Offset.Z) with y down.
        pos += Vec2(clock->offsetX, -(clock->offsetY - clock->offsetZ));
    }
    return pos;
}

void WorldScene::placeMobiles()
{
    auto& world = GameClient::instance().world();
    for (auto& [serial, node] : _entities)
    {
        if (const auto* e = world.get(serial); e != nullptr && e->isMobile())
        {
            node->setPosition(mobilePosition(*e));
        }
    }
}

Vec2 WorldScene::playerScreenPoint() const
{
    const auto* p = GameClient::instance().world().player();
    if (p == nullptr)
    {
        const auto size = _director->getVisibleSize();
        return Vec2(size.width / 2, size.height / 2);
    }
    // Director::screenToCanvas is an axis-aligned affine map (scale, flip, viewport); invert it.
    const Vec2 canvas = _worldNode->convertToWorldSpace(mobilePosition(*p));
    const Vec2 c0     = _director->screenToCanvas(Vec2::zero);
    const Vec2 cx     = _director->screenToCanvas(Vec2(1, 0)) - c0;
    const Vec2 cy     = _director->screenToCanvas(Vec2(0, 1)) - c0;
    if (cx.x == 0 || cy.y == 0)
    {
        return canvas;
    }
    return Vec2((canvas.x - c0.x) / cx.x, (canvas.y - c0.y) / cy.y);
}

std::optional<WorldScene::PickedTile> WorldScene::tileAt(Vec2 screen) const
{
    // The tile of whatever is drawn under the point: land, a static or a ground item.
    if (const auto* hit = pickAt(screen))
    {
        return PickedTile{hit->object->x, hit->object->y, hit->object->z};
    }

    auto& gc        = GameClient::instance();
    const auto* map = gc.install().map(currentMap());
    if (map == nullptr)
    {
        return std::nullopt;
    }

    // Nothing drawn there (outside the loaded blocks): invert tileToWorld. a = x - y,
    // b = x + y. Guess z = 0, then correct once for the land height of that tile.
    const Vec2 w = _worldNode->convertToNodeSpace(_director->screenToCanvas(screen));
    int z = 0;
    int x = 0, y = 0;
    for (int pass = 0; pass < 2; ++pass)
    {
        const float a = w.x / 22.f;
        const float b = (-w.y + z * 4.f) / 22.f;
        x = static_cast<int>(std::lround((a + b) / 2.f));
        y = static_cast<int>(std::lround((b - a) / 2.f));
        if (x < 0 || y < 0 || x >= map->width() || y >= map->height())
        {
            return std::nullopt;
        }
        z = map->land(x, y).z;
    }
    return PickedTile{x, y, z};
}

uo::world::Entity* WorldScene::entityAt(Vec2 screen) const
{
    auto& world = GameClient::instance().world();

    // Mobiles and ground items: only when one is the topmost thing drawn under the point.
    if (const auto* hit = pickAt(screen); hit != nullptr && hit->object->serial != 0)
    {
        return world.get(hit->object->serial);
    }
    return nullptr;
}

void WorldScene::onClick(MouseButton button, Vec2 screen)
{
    if (button != MouseButton::Left)
    {
        return;
    }

    auto& gc        = GameClient::instance();
    auto& targeting = gc.targeting();
    auto* entity    = entityAt(screen);

    if (targeting.isTargeting())
    {
        if (entity != nullptr)
        {
            targeting.target(entity->serial);
        }
        else if (const auto* hit = pickAt(screen))
        {
            // Land sends graphic 0; a static sends its graphic and, for surfaces, its height.
            const auto& o = *hit->object;
            uint16_t graphic = 0;
            uint8_t surface  = 0;
            if (o.kind == uo::render::ObjectKind::Static)
            {
                graphic = o.graphic;
                const auto data = _source->item(o.graphic);
                if (data.is(uo::assets::TF_Surface))
                    surface = data.height;
            }
            targeting.target(graphic, o.x, o.y, o.z, surface);
        }
        return;
    }

    if (entity != nullptr)
    {
        gc.singleClick(entity->serial);
    }
}

void WorldScene::onDoubleClick(MouseButton button, Vec2 screen)
{
    auto& gc = GameClient::instance();
    if (button == MouseButton::Left)
    {
        if (auto* entity = entityAt(screen))
        {
            gc.doubleClick(entity->serial);
        }
    }
    else if (button == MouseButton::Right)
    {
        // Right double-click walks to the tile (ClassicUO's pathfind on double right-click).
        if (auto tile = tileAt(screen))
        {
            gc.movement().walkTo(tile->x, tile->y, tile->z, 0);
        }
    }
}

void WorldScene::onEscape()
{
    auto& gc = GameClient::instance();
    if (gc.targeting().isTargeting())
    {
        gc.targeting().cancel();
        return;
    }

    auto& autoWalk = gc.movement().autoWalker();
    if (autoWalk.active() && autoWalk.cancellable())
    {
        gc.movement().stopAutoWalk();
        return;
    }

    // Nothing to cancel: leave the world for the login screen.
    gc.session().stop();
    _director->replaceScene(utils::createInstance<LoginScene>());
}

void WorldScene::centerCamera()
{
    const auto* p = GameClient::instance().world().player();
    if (!p)
        return;
    auto size   = _director->getVisibleSize();
    auto origin = _director->getVisibleOrigin();
    // Follow the player's step offset so the view scrolls smoothly (ClassicUO adds the
    // player's Offset to the camera) instead of snapping a tile when each step completes.
    const Vec2 player = mobilePosition(*p);
    _worldNode->setPosition(origin + Vec2(size.width / 2, size.height / 2) - player);
    // The draw list is built around the player's tile; shift the renderer by the same offset.
    // pickAt converts through the renderer's node space, so picking stays aligned.
    if (_renderer)
        _renderer->setPosition(origin - (player - tileToWorld(p->x, p->y, p->z)));
}

void WorldScene::streamBlocks()
{
    auto& gc        = GameClient::instance();
    const auto* p   = gc.world().player();
    const auto* map = gc.install().map(currentMap());
    if (!p || !map || !_map)
        return;

    int pbx = p->x >> 3, pby = p->y >> 3;
    std::set<BlockKey> wanted;
    for (int bx = pbx - kViewBlocks; bx <= pbx + kViewBlocks; ++bx)
        for (int by = pby - kViewBlocks; by <= pby + kViewBlocks; ++by)
            if (bx >= 0 && by >= 0 && bx < map->blockWidth() && by < map->blockHeight())
                wanted.insert({bx, by});

    for (auto it = _blocks.begin(); it != _blocks.end();)
    {
        if (!wanted.count(*it))
        {
            _map->unloadBlock(it->bx, it->by);
            it = _blocks.erase(it);
        }
        else
        {
            ++it;
        }
    }

    for (const auto& key : wanted)
        if (!_blocks.count(key))
            loadBlock(key);

    _drawListDirty = true;
}

void WorldScene::loadBlock(BlockKey key)
{
    _map->loadBlock(key.bx, key.by);
    _blocks.insert(key);

    // Ground items that arrived before their block was loaded (unloading dropped them).
    for (auto it = _items.begin(); it != _items.end();)
    {
        if ((it->second.first >> 3) == key.bx && (it->second.second >> 3) == key.by)
            it = _items.erase(it);
        else
            ++it;
    }
    GameClient::instance().world().forEachItem([&](uo::world::Item& i) {
        if (i.onGround() && (i.x >> 3) == key.bx && (i.y >> 3) == key.by)
            addItem(i);
    });
    // Mobiles on the block, likewise.
    GameClient::instance().world().forEachMobile([&](uo::world::Mobile& m) {
        if ((m.x >> 3) == key.bx && (m.y >> 3) == key.by)
        {
            _mobiles.erase(m.serial);
            addMobile(m);
        }
    });
}

void WorldScene::addMobile(const uo::world::Mobile& m)
{
    if (!_map)
        return;

    if (auto it = _mobiles.find(m.serial); it != _mobiles.end())
    {
        if (it->second.x == m.x && it->second.y == m.y && it->second.z == m.z)
            return;
        _map->removeObject(m.serial, it->second.x, it->second.y);
        _mobiles.erase(it);
    }

    uo::render::WorldObject obj;
    obj.kind    = uo::render::ObjectKind::Mobile;
    obj.graphic = m.graphic;
    obj.hue     = m.hue;
    obj.x       = m.x;
    obj.y       = m.y;
    obj.z       = m.z;
    obj.serial  = m.serial;

    if (_map->addObject(obj))
        _mobiles[m.serial] = {m.x, m.y, m.z};
    _drawListDirty = true;
}

void WorldScene::syncMobileTiles()
{
    if (!_map)
        return;
    GameClient::instance().world().forEachMobile([this](uo::world::Mobile& m) {
        const auto it = _mobiles.find(m.serial);
        if (it == _mobiles.end() || it->second.x != m.x || it->second.y != m.y || it->second.z != m.z)
            addMobile(m);
    });
}

void WorldScene::addItem(const uo::world::Entity& e)
{
    if (auto it = _items.find(e.serial); it != _items.end())
    {
        _map->removeObject(e.serial, it->second.first, it->second.second);
        _items.erase(it);
    }

    uo::render::WorldObject obj;
    obj.kind    = uo::render::ObjectKind::Item;
    obj.graphic = e.graphic;
    obj.hue     = e.hue;
    obj.x       = e.x;
    obj.y       = e.y;
    obj.z       = e.z;
    obj.serial  = e.serial;

    const auto& install = GameClient::instance().install();
    const auto* item    = e.isItem() ? static_cast<const uo::world::Item*>(&e) : nullptr;
    if (item && item->isMulti)
    {
        // Item.LoadMulti: the visible components go on the map as multi parts; an invisible
        // first component is what the item itself draws (MultiGraphic), and nothing when it is 0-2.
        static const std::vector<uo::assets::MultiComponent> kNone;
        const auto* multis = install.multis();
        const auto& parts  = multis ? multis->components(e.graphic) : kNone;
        _map->setMulti(e.serial, e.x, e.y, e.z, e.hue, parts);
        obj.graphic       = !parts.empty() && !parts.front().visible ? parts.front().graphic : 0;
        obj.allowedToDraw = obj.graphic > 2;
    }
    else if (const auto* data = install.tileData().staticTile(e.graphic))
    {
        obj.allowedToDraw = uo::render::canDrawStatic(e.graphic, {data->flags, data->height, data->name});
    }

    if (_map->addObject(obj))
        _items[e.serial] = {e.x, e.y};
    _drawListDirty = true;
}

void WorldScene::rebuildDrawList()
{
    const auto* p = GameClient::instance().world().player();
    if (!_map || !_renderer || !p)
        return;

    // Rebuilt when the player moves or the map or items change.
    if (!_drawListDirty && _viewX == p->x && _viewY == p->y && _viewZ == p->z)
        return;
    _viewX = p->x, _viewY = p->y, _viewZ = p->z;
    _drawListDirty = false;

    // Player's tile centred in the renderer node (ClassicUO's _offset).
    auto size = _director->getVisibleSize();
    uo::render::ViewParams view;
    view.offsetX  = (p->x - p->y) * 22 - static_cast<int>(size.width / 2);
    view.offsetY  = (p->x + p->y) * 22 - p->z * 4 - static_cast<int>(size.height / 2);
    view.minTileX = ((p->x >> 3) - kViewBlocks) * 8;
    view.minTileY = ((p->y >> 3) - kViewBlocks) * 8;
    view.maxTileX = ((p->x >> 3) + kViewBlocks) * 8 + 7;
    view.maxTileY = ((p->y >> 3) + kViewBlocks) * 8 + 7;
    view.playerZ  = p->z;
    view.animatedStatics = _animatedStatics.get();

    // Under a roof or an upper floor, hide what is above the player (ClassicUO UpdateMaxDrawZ).
    const auto limits = _map->computeViewZ(p->x, p->y, p->z);
    view.maxZ         = limits.maxZ;
    view.maxGroundZ   = limits.maxGroundZ;
    view.hideRoofs    = limits.hideRoofs;

    _map->buildDrawList(view, _drawList, _animator.get());
}

const uo::render::DrawItem* WorldScene::pickAt(Vec2 screen) const
{
    if (!_renderer || !_hitTest || !_source)
        return nullptr;

    // The draw list is y-down from the renderer node's top-left.
    const Vec2 local = _renderer->convertToNodeSpace(_director->screenToCanvas(screen));
    const int x      = static_cast<int>(std::floor(local.x));
    const int y      = static_cast<int>(std::floor(_renderer->getContentSize().height - local.y));
    return uo::render::pick(_drawList, *_source, *_hitTest, x, y);
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
    if (item)
    {
        // Ground items are sorted and hued with the statics by the renderer.
        if (_map)
            addItem(e);
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
            // The body is drawn by the renderer; this node carries the notoriety-coloured name.
            node      = Node::create();
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
        addMobile(*mobile);
        // Notoriety can arrive after creation (0x78 fills it in after the entity exists).
        if (auto* name = dynamic_cast<Label*>(node->getChildByName("name")))
        {
            name->setString(e.name);
            name->setTextColor(notorietyColor(mobile->notoriety));
        }
        node->setPosition(mobilePosition(e));
    }
    else
    {
        node->setPosition(tileToWorld(e.x, e.y, e.z) - Vec2(0, 22));
    }
    node->setLocalZOrder(depth(e.x, e.y, e.z, 2));
}

void WorldScene::removeEntity(std::uint32_t serial)
{
    if (_map && _map->removeMulti(serial))
        _drawListDirty = true;
    if (auto it = _items.find(serial); it != _items.end())
    {
        if (_map)
            _map->removeObject(serial, it->second.first, it->second.second);
        _items.erase(it);
        _drawListDirty = true;
    }
    if (auto it = _mobiles.find(serial); it != _mobiles.end())
    {
        if (_map)
            _map->removeObject(serial, it->second.x, it->second.y);
        _mobiles.erase(it);
        _drawListDirty = true;
    }
    if (auto it = _entities.find(serial); it != _entities.end())
    {
        it->second->removeFromParent();
        _entities.erase(it);
    }
    if (_overhead)
        _overhead->removeOwner(serial);
}

std::string WorldScene::messageText(const uo::world::Message& j) const
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
    return text;
}

std::string WorldScene::journalText(const uo::world::Message& j) const
{
    std::string text = messageText(j);
    if (!j.name.empty() && j.serial != 0xFFFFFFFF && j.name != "System")
        return j.name + ": " + text;
    return text;
}

void WorldScene::appendJournal(const uo::world::Message& m)
{
    if (!_journalView)
    {
        refreshJournal();
        return;
    }
    uo::client::text::JournalLine line;
    std::string text = journalText(m);
    if (!m.name.empty() && m.serial != 0xFFFFFFFF && m.name != "System")
    {
        line.name = m.name;
        text      = text.substr(m.name.size() + 2);
    }
    line.text    = std::move(text);
    line.hue     = m.hue;
    line.unicode = m.unicode;
    _journalView->append(line);
}

void WorldScene::refreshJournal()
{
    const auto& journal = GameClient::instance().world().journal();
    if (_journalView)
    {
        _journalView->clearEntries();
        for (const auto& m : journal)
            appendJournal(m);
        return;
    }
    std::string out;
    std::size_t start = journal.size() > 8 ? journal.size() - 8 : 0;
    for (std::size_t i = start; i < journal.size(); ++i)
    {
        out += journalText(journal[i]);
        out += '\n';
    }
    _journal->setString(out);
}

void WorldScene::showOverhead(const uo::world::Message& m)
{
    using uo::world::MessageType;

    if (!_overhead || m.serial == 0 || m.serial == 0xFFFFFFFF)
        return;

    // What ClassicUO puts over an object; guild, alliance, party, command and system text
    // stays in the journal.
    switch (m.type)
    {
    case MessageType::Regular:
    case MessageType::Emote:
    case MessageType::Label:
    case MessageType::Focus:
    case MessageType::Whisper:
    case MessageType::Yell:
    case MessageType::Spell:
    case MessageType::Limit3Spell:
    case MessageType::Encoded:
        break;
    default:
        return;
    }

    if (!GameClient::instance().world().get(m.serial))
        return;

    const std::string text = messageText(m);
    const auto font = uo::text::speechFont(m.font, m.unicode, uo::client::text::TextSystem::instance().fonts());
    _overhead->addMessage(m.serial, text, m.hue, font.font, font.unicode, m.type, m.textType);
}

std::optional<Vec2> WorldScene::overheadAnchor(std::uint32_t serial) const
{
    auto& gc      = GameClient::instance();
    const auto* e = gc.world().get(serial);
    if (!e)
        return std::nullopt;

    Vec2 local;
    if (e->isMobile())
    {
        local = mobilePosition(*e) + Vec2(0, kMobileTextHeight);
    }
    else
    {
        const auto* item = static_cast<const uo::world::Item*>(e);
        if (!item->onGround())
            return std::nullopt;
        // Ground items stand on the tile centre (see syncEntity); text starts at the art's top.
        float height = 44.f;
        if (Texture2D* tex = gc.textures().statik(e->graphic))
            height = tex->getContentSize().height;
        local = tileToWorld(e->x, e->y, e->z) + Vec2(0, height - 22.f);
    }

    const Vec2 world  = _worldNode->convertToWorldSpace(local);
    const auto size   = _director->getVisibleSize();
    const auto origin = _director->getVisibleOrigin();
    return Vec2(world.x - origin.x, size.height - (world.y - origin.y));
}
