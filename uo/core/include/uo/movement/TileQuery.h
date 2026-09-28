// SPDX-License-Identifier: BSD-2-Clause
// Engine-free view of a map tile for walk checks. The game-state layer, which owns the world's
// tile lists, fills these; the pathfinder never touches world objects directly.

#pragma once

#include "uo/assets/TileData.h"

#include <array>
#include <cstdint>
#include <vector>

namespace uo::movement
{

// TileData flag bits the walk code reads.
namespace tile_flag
{
inline constexpr uint64_t Impassable = assets::TF_Impassable;
inline constexpr uint64_t Wet        = assets::TF_Wet;
inline constexpr uint64_t Surface    = assets::TF_Surface;
inline constexpr uint64_t Bridge     = assets::TF_Bridge;
inline constexpr uint64_t Internal   = assets::TF_Internal;
inline constexpr uint64_t Door       = assets::TF_Door;
inline constexpr uint64_t NoDiagonal = assets::TF_NoDiagonal;
}  // namespace tile_flag

// How the player moves, which changes what blocks them.
enum class StepState : uint8_t
{
    Normal,
    DeadOrGM,
    OnSeaHorse,
    Flying,
};

inline constexpr uint16_t kGameMasterBody = 0x03DB;
inline constexpr uint16_t kSeaHorseMount  = 0x3EB3;

constexpr StepState stepStateFor(bool dead, uint16_t body, bool gargoyleFlying, uint16_t mountGraphic)
{
    if (dead || body == kGameMasterBody)
    {
        return StepState::DeadOrGM;
    }
    if (gargoyleFlying)
    {
        return StepState::Flying;
    }
    if (mountGraphic == kSeaHorseMount)
    {
        return StepState::OnSeaHorse;
    }
    return StepState::Normal;
}

enum class TileObjectKind : uint8_t
{
    Land,
    Static,
    Item,
    Multi,
    Mobile,
    Effect,
};

// One object on a tile, as the walk code needs to see it. Fill only the fields for its kind.
struct TileObject
{
    TileObjectKind kind{TileObjectKind::Static};
    uint16_t graphic{0};  // land or art graphic; for a multi item, its multi graphic
    int8_t z{0};

    // Land, static, item and multi: tiledata flags and height for `graphic`.
    uint64_t tileFlags{0};
    uint8_t height{0};

    // Land only. Corner heights are {top (the tile's own z), right, bottom, left}.
    int8_t landMinZ{0};
    int8_t landAverageZ{0};
    bool landStretched{false};
    std::array<int8_t, 4> landCornerZ{};

    // Item only.
    bool itemIsMulti{false};
    bool itemLocked{false};
    uint8_t itemWeight{0};

    // Multi component only.
    bool multiIsCustom{false};
    bool multiGenericInternal{false};  // CHMOF_GENERIC_INTERNAL
    bool multiIgnoreInRender{false};   // CHMOF_IGNORE_IN_RENDER
    bool multiHousePreview{false};

    // Mobile only.
    bool mobileDead{false};
    bool mobileIgnoresCharacters{false};

    bool has(uint64_t flag) const { return (tileFlags & flag) != 0; }
};

// Supplies the objects on a tile. Return false when the tile is not loaded; the tile then blocks.
class ITileSource
{
public:
    virtual ~ITileSource() = default;
    virtual bool gatherTile(int x, int y, std::vector<TileObject>& out) = 0;
};

// Everything about the player and options that walk checks depend on.
struct WalkerContext
{
    StepState stepState{StepState::Normal};
    bool isGameMaster{false};      // body 0x03DB
    int8_t playerZ{0};
    bool ignoreCharacters{false};  // see ignoresCharacters()
    bool smoothDoors{false};       // profile option: doors never block

    // House customization: while editing, only objects at or above the player's z count and
    // the player cannot leave the foundation rectangle.
    bool customHouseEditing{false};
    int customHouseStartX{0}, customHouseStartY{0}, customHouseEndX{0}, customHouseEndY{0};

    // ClassicUO's rule for walking through other characters: allowed when the stamina check is
    // off, when dead or a GM, when the player has the ignore-characters flag, or unless the player
    // is below full stamina in Felucca (map 0).
    static bool ignoresCharacters(bool ignoreStaminaCheck, StepState state, bool playerIgnoresCharacters,
                                  int stamina, int staminaMax, int mapIndex)
    {
        return ignoreStaminaCheck || state == StepState::DeadOrGM || playerIgnoresCharacters ||
               !(stamina < staminaMax && mapIndex == 0);
    }
};

}  // namespace uo::movement
