// SPDX-License-Identifier: MIT
#pragma once

#include "axmol/axmol.h"

#include "uo/world/World.h"

#include <cstdint>
#include <map>
#include <set>
#include <unordered_map>
#include <vector>

// The game view: isometric terrain and statics streamed in 8x8 map blocks around the player,
// world items and mobiles mirrored from uo::world::World, keyboard walking and a journal.
// The Axmol version of ClassicUO's GameScene; mobiles are placeholders until the animation
// port lands.
class WorldScene : public ax::Scene
{
public:
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
    void handleWalking(float dt);
    void refreshJournal();
    std::string journalText(const uo::world::Message& m) const;

    void onKeyPressed(ax::KeyboardEvent* ev);
    void onKeyReleased(ax::KeyboardEvent* ev);

    ax::Node* _worldNode = nullptr;
    ax::Label* _journal  = nullptr;
    ax::Label* _coords   = nullptr;
    ax::KeyboardEventListener* _keys = nullptr;

    std::map<BlockKey, std::vector<ax::Node*>> _blocks;
    std::unordered_map<std::uint32_t, ax::Node*> _entities;
    std::set<ax::KeyboardEvent::KeyCode> _held;
    float _walkCooldown = 0;
    bool _running       = false;
    bool _quitting      = false;
    int _lastPlayerX = -1, _lastPlayerY = -1;

    static constexpr int kViewBlocks = 3;  // blocks loaded on each side of the player's block
};
