// SPDX-License-Identifier: BSD-2-Clause
// Part of the AxmolUO client. Ported from ClassicUO Game/Data/ContainerData.cs and
// Game/Managers/ContainerManager.cs (the built-in container table and the default cascade
// that places newly opened containers).
#pragma once

#include <cstdint>

namespace uo::client::gumps
{

// A container gump's layout: where items may sit, the sounds it plays and its minimised form.
// Bounds are LEFT, TOP, RIGHT, BOTTOM in gump pixels (ClassicUO stores them in a Rectangle
// but uses Width/Height as the right and bottom edges).
struct ContainerData
{
    uint16_t graphic = 0;
    uint16_t openSound = 0;
    uint16_t closedSound = 0;
    int left = 44;
    int top = 65;
    int right = 186;
    int bottom = 159;
    // Gump shown while minimised; 0 when the container cannot be minimised.
    uint16_t iconizedGraphic = 0;
    // 16x16 click area that minimises the gump; all zero when there is none.
    int minimizerX = 0;
    int minimizerY = 0;
    int minimizerWidth = 0;
    int minimizerHeight = 0;

    bool hasMinimizer() const { return minimizerWidth > 0 && minimizerHeight > 0; }
};

// ContainerManager.Get: the entry for a gump id, or ClassicUO's default (44, 65, 186, 159,
// no sounds) for an id the table does not know.
ContainerData containerData(uint16_t gumpGraphic);

// Gump ids with special item drawing (items are gumps, graphic - kItemGumpTextureOffset).
inline constexpr uint16_t kChessboardGump = 0x091A;
inline constexpr uint16_t kBackgammonGump = 0x092E;
inline constexpr uint16_t kCorpseGump = 0x0009;
inline constexpr int kItemGumpTextureOffset = 11369;

// ContainerManager.CalculateContainerPosition without a saved position and without the
// OverrideContainerLocation profile option: containers cascade by 20 pixels from (40, 40)
// and wrap to the next line or back to the start at the screen edges. One instance per session.
class ContainerPlacement
{
public:
    // Top-left for a new container gump of the given size on a screen of the given size.
    void next(int gumpWidth, int gumpHeight, int screenWidth, int screenHeight, int& outX, int& outY);

    void reset()
    {
        _x = kDefault;
        _y = kDefault;
    }

private:
    static constexpr int kDefault = 40;
    static constexpr int kStep = 20;
    static constexpr int kLineStep = 800;

    int _x = kDefault;
    int _y = kDefault;
};

}  // namespace uo::client::gumps
