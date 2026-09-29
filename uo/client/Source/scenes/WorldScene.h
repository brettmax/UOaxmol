// SPDX-License-Identifier: MIT
#pragma once

#include "axmol/axmol.h"

#include "uo/assets/AnimData.h"
#include "uo/assets/Texmaps.h"
#include "uo/render/AnimatedStatics.h"
#include "uo/render/Pick.h"
#include "uo/render/WorldMap.h"
#include "uo/render/WorldSource.h"
#include "uo/world/World.h"

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <unordered_map>
#include <vector>

namespace uo::client::text
{
class OverheadTextLayer;
}

class SystemChat;

namespace uo::client::input
{
class InputRouter;
enum class MouseButton : uint8_t;
}  // namespace uo::client::input

namespace uo::render
{
class WorldRenderer;
class ITextureSource;
}  // namespace uo::render

namespace uo::anim
{
class AnimationsLoader;
class AnimationCache;
class WorldMobileAnimator;
}  // namespace uo::anim

namespace uo::client
{
class AnimationTextures;
}

// The game view: terrain, statics and ground items drawn by uo::render::WorldRenderer
// (stretched land, ClassicUO depth order, GPU hues) over 8x8 map blocks streamed around the
// player, mobiles mirrored from uo::world::World, mouse and keyboard input (uo/client/input)
// driving movement and the target cursor, and the chat line with recent system lines. Mobiles are animated
// (uo::anim::WorldMobileAnimator) and drawn in the same depth-sorted list. The Axmol version of
// ClassicUO's GameScene.
class WorldScene : public ax::Scene
{
public:
    WorldScene();
    ~WorldScene() override;

    bool init() override;
    void onEnter() override;
    void onExit() override;
    void update(float dt) override;

    // Screen position (y-up, world-node space) of the centre of tile (x, y) at height z.
    static ax::Vec2 tileToWorld(int x, int y, int z);
    // Draw order: back rows first, then by height, land below everything on its tile.
    static int depth(int x, int y, int z, int bias);

    // Topmost land tile, static, ground item or mobile under a screen point (top-left origin, as
    // InputRouter reports them), with ClassicUO's selection rules (uo/render/Pick.h). Its object
    // carries kind, graphic, tile x/y/z and serial (0 for land and statics). nullptr when nothing
    // is there. Valid until the next update().
    const uo::render::DrawItem* pickAt(ax::Vec2 screen) const;

private:
    struct BlockKey
    {
        int bx, by;
        bool operator<(const BlockKey& o) const { return bx != o.bx ? bx < o.bx : by < o.by; }
    };

    void streamBlocks();
    void loadBlock(BlockKey key);
    void addItem(const uo::world::Entity& e);
    // Puts a mobile in the world map on its current tile (re-sorting it when it moved).
    void addMobile(const uo::world::Mobile& m);
    // Re-files mobiles whose tile changed since they were added (steps complete in updateMovement).
    void syncMobileTiles();
    void rebuildDrawList();
    void syncEntity(const uo::world::Entity& e);
    void removeEntity(std::uint32_t serial);
    void centerCamera();
    // A message's text with cliloc fallback applied, without the speaker's name.
    std::string messageText(const uo::world::Message& m) const;
    // Speech and labels over the mobile or ground item that sent them (MessageManager).
    void showOverhead(const uo::world::Message& m);
    // Where an entity's overhead text stacks from, in UO screen pixels (top-left origin).
    std::optional<ax::Vec2> overheadAnchor(std::uint32_t serial) const;

    // Draw position of a mobile this frame: its tile plus the step it is part-way through.
    ax::Vec2 mobilePosition(const uo::world::Entity& e) const;
    void placeMobiles();

    // Input (screen points, top-left origin, as InputRouter reports them).
    ax::Vec2 playerScreenPoint() const;
    struct PickedTile
    {
        int x, y, z;
    };
    std::optional<PickedTile> tileAt(ax::Vec2 screen) const;
    uo::world::Entity* entityAt(ax::Vec2 screen) const;
    void onClick(uo::client::input::MouseButton button, ax::Vec2 screen);
    void onDoubleClick(uo::client::input::MouseButton button, ax::Vec2 screen);
    void onEscape();

    ax::Node* _worldNode = nullptr;
    SystemChat* _chat = nullptr;  // chat line and recent system lines, bottom left
    uo::client::text::OverheadTextLayer* _overhead = nullptr;  // null without UO fonts
    ax::Label* _coords   = nullptr;
    std::unique_ptr<uo::client::input::InputRouter> _input;

    std::unique_ptr<uo::assets::Texmaps> _texmaps;
    std::unique_ptr<uo::render::AssetsWorldSource> _source;
    std::unique_ptr<uo::render::WorldMap> _map;
    // Animations: the cache owns the frames the draw list points at; textures and the animator
    // are declared after it so they are destroyed first.
    std::unique_ptr<uo::anim::AnimationsLoader> _animLoader;
    std::unique_ptr<uo::anim::AnimationCache> _animCache;
    std::unique_ptr<uo::client::AnimationTextures> _animTextures;
    std::unique_ptr<uo::anim::WorldMobileAnimator> _animator;
    std::unique_ptr<uo::render::ITextureSource> _textureSource;
    uo::render::WorldRenderer* _renderer = nullptr;
    std::vector<uo::render::DrawItem> _drawList;
    std::unique_ptr<uo::render::ArtHitTest> _hitTest;
    std::unique_ptr<uo::assets::AnimData> _animData;
    std::unique_ptr<uo::render::AnimatedStatics> _animatedStatics;
    bool _drawListDirty = true;
    int _viewX = -1, _viewY = -1, _viewZ = -1000;

    std::set<BlockKey> _blocks;
    // Ground items drawn by the renderer, with the tile they were added at.
    std::unordered_map<std::uint32_t, std::pair<int, int>> _items;
    // Mobile name labels, drawn above the world.
    std::unordered_map<std::uint32_t, ax::Node*> _entities;
    // Mobiles in the world map, with the tile they were filed on.
    struct MobileTile
    {
        int x, y, z;
    };
    std::unordered_map<std::uint32_t, MobileTile> _mobiles;
    bool _quitting = false;
    int _lastPlayerX = -1, _lastPlayerY = -1;

    static constexpr int kViewBlocks = 3;  // blocks loaded on each side of the player's block
};
