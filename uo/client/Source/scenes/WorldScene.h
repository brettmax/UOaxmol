// SPDX-License-Identifier: MIT
#pragma once

#include "axmol/axmol.h"

#include "uo/assets/Texmaps.h"
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
class JournalView;
}

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

// The game view: terrain, statics and ground items drawn by uo::render::WorldRenderer
// (stretched land, ClassicUO depth order, GPU hues) over 8x8 map blocks streamed around the
// player, mobiles mirrored from uo::world::World, mouse and keyboard input (uo/client/input)
// driving movement and the target cursor, and a journal. The Axmol version of ClassicUO's
// GameScene; mobiles are placeholders until the animation port lands.
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

    // Topmost land tile, static or ground item under a screen point (top-left origin, as
    // InputRouter reports them), with ClassicUO's selection rules (uo/render/Pick.h). Its object
    // carries kind, graphic, tile x/y/z and serial (0 for land and statics). nullptr when nothing
    // is there. Valid until the next update(). Mobiles are not in the draw list yet; entityAt
    // checks their markers first.
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
    void rebuildDrawList();
    void syncEntity(const uo::world::Entity& e);
    void removeEntity(std::uint32_t serial);
    void centerCamera();
    void refreshJournal();
    std::string journalText(const uo::world::Message& m) const;
    void appendJournal(const uo::world::Message& m);

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
    ax::Label* _journal  = nullptr;  // TTF fallback when UO fonts are missing
    uo::client::text::JournalView* _journalView = nullptr;
    ax::Label* _coords   = nullptr;
    std::unique_ptr<uo::client::input::InputRouter> _input;

    std::unique_ptr<uo::assets::Texmaps> _texmaps;
    std::unique_ptr<uo::render::AssetsWorldSource> _source;
    std::unique_ptr<uo::render::WorldMap> _map;
    std::unique_ptr<uo::render::ITextureSource> _textureSource;
    uo::render::WorldRenderer* _renderer = nullptr;
    std::vector<uo::render::DrawItem> _drawList;
    std::unique_ptr<uo::render::ArtHitTest> _hitTest;
    bool _drawListDirty = true;
    int _viewX = -1, _viewY = -1, _viewZ = -1000;

    std::set<BlockKey> _blocks;
    // Ground items drawn by the renderer, with the tile they were added at.
    std::unordered_map<std::uint32_t, std::pair<int, int>> _items;
    // Mobiles (placeholder nodes), drawn above the world.
    std::unordered_map<std::uint32_t, ax::Node*> _entities;
    bool _quitting = false;
    int _lastPlayerX = -1, _lastPlayerY = -1;

    static constexpr int kViewBlocks = 3;  // blocks loaded on each side of the player's block
};
