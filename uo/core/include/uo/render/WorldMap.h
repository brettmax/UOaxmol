// SPDX-License-Identifier: BSD-2-Clause
// Ported from ClassicUO (Game/Map/Chunk.cs, GameObjects/Land.cs, Static.cs,
// View.cs, Scenes/RenderLists.cs).
//
// Engine-free world model for rendering: 8x8 blocks of tiles, each tile holding
// its objects ordered the way ClassicUO orders them (priority Z, land before
// equal-priority items), and a draw list builder that sorts everything
// back-to-front with ClassicUO's depth key.
//
// ClassicUO draws with a depth buffer and a per-object constant depth. Painting
// in ascending depth order, with ties broken by draw order, produces the same
// opaque result and handles translucent pixels better, so the Axmol side just
// submits the list in order.

#pragma once

#include <array>
#include <cstdint>
#include <unordered_map>
#include <vector>

#include "uo/render/HueVector.h"
#include "uo/render/LandStretch.h"
#include "uo/render/WorldSource.h"

namespace uo::render
{

enum class ObjectKind : uint8_t
{
    Land,
    Static,
    Item,
    Corpse,
    Mobile,
    Multi,
    Effect,
};

// Custom house multi component state (CUSTOM_HOUSE_MULTI_OBJECT_FLAGS subset).
enum MultiState : uint8_t
{
    MULTI_NONE             = 0,
    MULTI_GENERIC_INTERNAL = 0x01,
    MULTI_PREVIEW          = 0x02,
};

struct WorldObject
{
    ObjectKind kind  = ObjectKind::Static;
    uint16_t graphic = 0;
    uint16_t hue     = 0;
    uint16_t x       = 0;
    uint16_t y       = 0;
    int8_t z         = 0;
    uint8_t alpha    = 0xFF;
    uint8_t multiState = MULTI_NONE;
    bool allowedToDraw = true;
    bool canBeTransparent = false;  // eligible for the circle of transparency
    int16_t priorityZ = 0;
    uint32_t serial   = 0;          // 0 for land and statics
    LandStretch land;               // only meaningful for ObjectKind::Land
};

struct ViewParams
{
    // Inclusive tile range to draw.
    int minTileX = 0, minTileY = 0, maxTileX = 0, maxTileY = 0;

    // Screen offset subtracted from every object's isometric position
    // (ClassicUO's _offset: the camera's top-left in world pixels).
    int offsetX = 0, offsetY = 0;

    // Objects (not land) at or above this Z are skipped: roofs and upper floors
    // when the player is indoors. 127 draws everything.
    int maxZ = 127;

    // A tile's list stops at the first object whose priority Z is above this
    // (ClassicUO's _maxGroundZ, set when the player stands under ground).
    int maxGroundZ = 127;

    // Skip roof tiles (player indoors, or roofs turned off).
    bool hideRoofs = false;

    // Circle of transparency: statics that pass TransparentTest(playerZ + 5)
    // are flagged so the shader cuts a hole around the player.
    bool circleOfTransparency = false;
    int playerZ = 0;

    // When non-zero every object is drawn in this hue (dead grey, highlight).
    uint16_t overrideHue = 0;

    // Draw a flattened shadow under foliage statics.
    bool staticShadows = false;
};

enum class DrawType : uint8_t
{
    LandFlat,       // 44x44 art diamond, graphic is the land id
    LandStretched,  // texmap quad, graphic is the land id (texId via tiledata)
    Static,         // item art, graphic is the item id; anchor with staticDrawOrigin
    Shadow,         // flattened item art under a static
};

struct DrawItem
{
    DrawType type    = DrawType::Static;
    uint16_t graphic = 0;
    int screenX      = 0;  // ClassicUO RealScreenPosition, y down
    int screenY      = 0;
    float depth      = 0;
    HueVector hue;
    const WorldObject* object = nullptr;
};

// Top-left of an item art sprite of the given size for an object at
// (screenX, screenY). Matches GameObject.DrawStatic.
inline void staticDrawOrigin(int screenX, int screenY, int artWidth, int artHeight, int& outX, int& outY)
{
    outX = screenX - ((artWidth >> 1) - 22);
    outY = screenY - (artHeight - 44);
}

// Isometric screen position (GameObject.UpdateRealScreenPosition).
inline void isoScreenPosition(int x, int y, int z, int offsetX, int offsetY, int& outX, int& outY)
{
    outX = ((x - y) * 22) - offsetX - 22;
    outY = ((x + y) * 22 - (z << 2)) - offsetY - 22;
}

// Sort key (GameObject.CalculateDepthZ) for an object with no movement offset.
inline float depthKey(int x, int y, int priorityZ)
{
    return static_cast<float>(x + y) + (127 + priorityZ) * 0.01f;
}

// ClassicUO GameObject.CanBeDrawn for statics.
bool canDrawStatic(uint16_t graphic, const StaticTileData& data, bool gargoyle = false);

// Priority Z and tile-list insertion state (Chunk.AddGameObject). state 0 is
// land, 1 a multi component, 2 a multi preview, -1 anything else.
struct Priority
{
    int16_t priorityZ;
    int8_t state;
};

Priority computePriority(const WorldObject& obj, const ITileData& tiles);

class WorldMap
{
public:
    static constexpr int kBlockSize = 8;

    using Cell  = std::vector<WorldObject>;
    struct Block
    {
        int blockX = 0, blockY = 0;
        std::array<Cell, kBlockSize * kBlockSize> cells;
    };

    WorldMap(const IMapSource& map, const ITileData& tiles) : _map(map), _tiles(tiles) {}

    // Loads land and statics of a block (Chunk.Load). Reloading replaces it.
    Block& loadBlock(int blockX, int blockY);
    void unloadBlock(int blockX, int blockY);
    const Block* findBlock(int blockX, int blockY) const;

    // Loads every block overlapping the view's tile range that is not loaded.
    void ensureLoaded(const ViewParams& view);

    // Inserts a dynamic object (item, mobile, multi part, effect) in its tile's
    // ordered list. The block must be loaded. Returns false otherwise.
    bool addObject(WorldObject obj);

    // Builds the back-to-front draw list for the view. `out` is cleared.
    void buildDrawList(const ViewParams& view, std::vector<DrawItem>& out) const;

    const Cell* cellAt(int x, int y) const;

private:
    static uint64_t key(int bx, int by) { return (static_cast<uint64_t>(static_cast<uint32_t>(bx)) << 32) | static_cast<uint32_t>(by); }

    void insert(Cell& cell, WorldObject obj);

    const IMapSource& _map;
    const ITileData& _tiles;
    std::unordered_map<uint64_t, Block> _blocks;
};

}  // namespace uo::render
