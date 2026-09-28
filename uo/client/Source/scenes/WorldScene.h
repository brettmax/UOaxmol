// SPDX-License-Identifier: MIT
#pragma once

#include "axmol/axmol.h"

#include "uo/world/World.h"

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <unordered_map>
#include <vector>

// The game view: isometric terrain and statics streamed in 8x8 map blocks around the player,
// world items and mobiles mirrored from uo::world::World, mouse and keyboard input
// (uo/client/input) driving movement and the target cursor, and a journal.
// The Axmol version of ClassicUO's GameScene; mobiles are placeholders until the animation
// port lands.
namespace uo::client::text
{
class JournalView;
}

namespace uo::client::input
{
class InputRouter;
enum class MouseButton : uint8_t;
}  // namespace uo::client::input

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

private:
    struct BlockKey
    {
        int bx, by;
        bool operator<(const BlockKey& o) const { return bx != o.bx ? bx < o.bx : by < o.by; }
    };

    void streamBlocks();
    void buildBlock(BlockKey key);
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

    std::map<BlockKey, std::vector<ax::Node*>> _blocks;
    std::unordered_map<std::uint32_t, ax::Node*> _entities;
    bool _quitting = false;
    int _lastPlayerX = -1, _lastPlayerY = -1;

    static constexpr int kViewBlocks = 3;  // blocks loaded on each side of the player's block
};
