// SPDX-License-Identifier: BSD-2-Clause
// Part of the AxmolUO client. Ported from ClassicUO Game/Managers/ContainerManager.cs
// (MakeDefault, Get, CalculateContainerPosition).
#include "gumps/client/ContainerData.h"


namespace uo::client::gumps
{

namespace
{

struct Entry
{
    uint16_t graphic;
    uint16_t openSound;
    uint16_t closedSound;
    int left, top, right, bottom;
    uint16_t iconized;
    int minimizerX, minimizerY;
};

// ContainerManager.MakeDefault, in the same order.
constexpr Entry kTable[] = {
    {0x0007, 0x0000, 0x0000, 30, 30, 270, 170, 0, 0, 0},
    {0x0009, 0x0000, 0x0000, 20, 85, 124, 196, 0, 0, 0},
    {0x003C, 0x0048, 0x0058, 44, 65, 186, 159, 0x0050, 105, 162},
    {0x003D, 0x0048, 0x0058, 29, 34, 137, 128, 0, 0, 0},
    {0x003E, 0x002F, 0x002E, 33, 36, 142, 148, 0, 0, 0},
    {0x003F, 0x004F, 0x0058, 19, 47, 182, 123, 0, 0, 0},
    {0x0040, 0x002D, 0x002C, 16, 38, 152, 125, 0, 0, 0},
    {0x0041, 0x004F, 0x0058, 40, 30, 139, 123, 0, 0, 0},
    {0x0042, 0x002D, 0x002C, 18, 105, 162, 178, 0, 0, 0},
    {0x0043, 0x002D, 0x002C, 16, 51, 184, 124, 0, 0, 0},
    {0x0044, 0x002D, 0x002C, 20, 10, 170, 100, 0, 0, 0},
    {0x0047, 0x0000, 0x0000, 16, 10, 148, 138, 0, 0, 0},
    {0x0048, 0x002F, 0x002E, 16, 10, 154, 94, 0, 0, 0},
    {0x0049, 0x002D, 0x002C, 18, 105, 162, 178, 0, 0, 0},
    {0x004A, 0x002D, 0x002C, 18, 105, 162, 178, 0, 0, 0},
    {0x004B, 0x002D, 0x002C, 16, 51, 184, 124, 0, 0, 0},
    {0x004C, 0x002D, 0x002C, 46, 74, 196, 184, 0, 0, 0},
    {0x004D, 0x002F, 0x002E, 76, 12, 140, 68, 0, 0, 0},
    {0x004E, 0x002D, 0x002C, 24, 18, 100, 152, 0, 0, 0},
    {0x004F, 0x002D, 0x002C, 24, 18, 100, 152, 0, 0, 0},
    {0x0051, 0x002F, 0x002E, 16, 10, 154, 94, 0, 0, 0},
    {0x0052, 0x0000, 0x0000, 0, 0, 110, 62, 0, 0, 0},
    {0x0102, 0x004F, 0x0058, 35, 10, 190, 95, 0, 0, 0},
    {0x0103, 0x0048, 0x0058, 41, 21, 173, 104, 0, 0, 0},
    {0x0104, 0x002F, 0x002E, 10, 10, 160, 105, 0, 0, 0},
    {0x0105, 0x002F, 0x002E, 10, 10, 160, 105, 0, 0, 0},
    {0x0106, 0x002F, 0x002E, 10, 10, 160, 105, 0, 0, 0},
    {0x0107, 0x002F, 0x002E, 10, 10, 160, 105, 0, 0, 0},
    {0x0108, 0x004F, 0x0058, 10, 10, 160, 105, 0, 0, 0},
    {0x0109, 0x002D, 0x002C, 10, 10, 160, 105, 0, 0, 0},
    {0x010A, 0x002D, 0x002C, 10, 10, 160, 105, 0, 0, 0},
    {0x010B, 0x002D, 0x002C, 10, 10, 160, 105, 0, 0, 0},
    {0x010C, 0x002F, 0x002E, 10, 10, 160, 105, 0, 0, 0},
    {0x010D, 0x002F, 0x002E, 10, 10, 160, 105, 0, 0, 0},
    {0x010E, 0x002F, 0x002E, 10, 10, 160, 105, 0, 0, 0},
    {0x0116, 0x0000, 0x0000, 40, 25, 140, 110, 0, 0, 0},
    {0x011A, 0x0000, 0x0000, 10, 65, 125, 160, 0, 0, 0},
    {0x011B, 0x0000, 0x0000, 45, 10, 175, 95, 0, 0, 0},
    {0x011C, 0x0000, 0x0000, 37, 10, 175, 105, 0, 0, 0},
    {0x011D, 0x0000, 0x0000, 43, 10, 165, 110, 0, 0, 0},
    {0x011E, 0x0000, 0x0000, 30, 22, 263, 106, 0, 0, 0},
    {0x011F, 0x0000, 0x0000, 45, 10, 175, 95, 0, 0, 0},
    {0x0120, 0x0000, 0x0000, 56, 30, 160, 107, 0, 0, 0},
    {0x0121, 0x0000, 0x0000, 77, 32, 162, 107, 0, 0, 0},
    {0x0123, 0x0000, 0x0000, 36, 19, 111, 157, 0, 0, 0},
    {0x0484, 0x0000, 0x0000, 0, 45, 175, 125, 0, 0, 0},
    {0x058E, 0x0000, 0x0000, 50, 150, 348, 250, 0, 0, 0},
    {0x06D3, 0x0000, 0x0000, 10, 65, 125, 160, 0, 0, 0},
    {0x06D4, 0x0000, 0x0000, 10, 65, 125, 160, 0, 0, 0},
    {0x06D5, 0x0000, 0x0000, 10, 65, 125, 160, 0, 0, 0},
    {0x06D6, 0x0000, 0x0000, 10, 65, 125, 160, 0, 0, 0},
    {0x06E5, 0x0000, 0x0000, 66, 74, 306, 520, 0, 0, 0},
    {0x06E6, 0x0000, 0x0000, 66, 74, 306, 520, 0, 0, 0},
    {0x06E7, 0x0000, 0x0000, 50, 60, 548, 308, 0, 0, 0},
    {0x06E8, 0x0000, 0x0000, 50, 60, 548, 308, 0, 0, 0},
    {0x06E9, 0x0000, 0x0000, 60, 80, 318, 324, 0, 0, 0},
    {0x06EA, 0x0000, 0x0000, 50, 60, 548, 308, 0, 0, 0},
    {0x091A, 0x0000, 0x0000, 0, 0, 282, 230, 0, 0, 0},
    {0x092E, 0x0000, 0x0000, 0, 0, 282, 210, 0, 0, 0},
    {0x266A, 0x0000, 0x0000, 16, 51, 184, 124, 0, 0, 0},
    {0x266B, 0x0000, 0x0000, 16, 51, 184, 124, 0, 0, 0},
    {0x2A63, 0x0187, 0x01C9, 60, 33, 460, 348, 0, 0, 0},
    {0x4D0C, 0x0000, 0x0000, 25, 65, 220, 155, 0, 0, 0},
    {0x775E, 0x0048, 0x0058, 44, 65, 186, 159, 0x775F, 105, 178},
    {0x7760, 0x0048, 0x0058, 44, 65, 186, 159, 0x7761, 105, 178},
    {0x7762, 0x0048, 0x0058, 44, 65, 186, 159, 0x7763, 105, 178},
    {0x777A, 0x0000, 0x0000, 32, 40, 184, 116, 0, 0, 0},
    {0x9CD9, 0x0000, 0x0000, 10, 10, 160, 105, 0, 0, 0},
    {0x9CDB, 0x0000, 0x0000, 50, 60, 548, 308, 0, 0, 0},
    {0x9CDD, 0x0000, 0x0000, 50, 60, 548, 308, 0, 0, 0},
    {0x9CDF, 0x0000, 0x0000, 50, 60, 548, 308, 0, 0, 0},
    {0x9CE3, 0x0000, 0x0000, 50, 60, 548, 308, 0, 0, 0},
    {0x9CE4, 0x0000, 0x0000, 44, 65, 186, 159, 0, 0, 0},
    {0x9CE5, 0x0000, 0x0000, 44, 65, 186, 159, 0, 0, 0},
    {0x9CE7, 0x0000, 0x0000, 44, 65, 186, 159, 0, 0, 0},
};

}  // namespace

ContainerData containerData(uint16_t gumpGraphic)
{
    ContainerData d;
    d.graphic = gumpGraphic;

    if (gumpGraphic == 0)
    {
        return d;
    }

    for (const auto& e : kTable)
    {
        if (e.graphic != gumpGraphic)
        {
            continue;
        }

        d.openSound = e.openSound;
        d.closedSound = e.closedSound;
        d.left = e.left;
        d.top = e.top;
        d.right = e.right;
        d.bottom = e.bottom;
        d.iconizedGraphic = e.iconized;

        // ContainerData's constructor: no minimiser area when both coordinates are zero.
        if (e.minimizerX != 0 || e.minimizerY != 0)
        {
            d.minimizerX = e.minimizerX;
            d.minimizerY = e.minimizerY;
            d.minimizerWidth = 16;
            d.minimizerHeight = 16;
        }

        break;
    }

    return d;
}

void ContainerPlacement::next(int gumpWidth, int gumpHeight, int screenWidth, int screenHeight, int& outX, int& outY)
{
    int passed = 0;

    for (int i = 0; i < 4 && passed == 0; i++)
    {
        if (_x + gumpWidth + kStep > screenWidth)
        {
            _x = kDefault;

            if (_y + gumpHeight + kLineStep > screenHeight)
            {
                _y = kDefault;
            }
            else
            {
                _y += kLineStep;
            }
        }
        else if (_y + gumpHeight + kStep > screenHeight)
        {
            if (_x + gumpWidth + kLineStep > screenWidth)
            {
                _x = kDefault;
            }
            else
            {
                _x += kLineStep;
            }

            _y = kDefault;
        }
        else
        {
            passed = i + 1;
        }
    }

    if (passed == 0)
    {
        _x = kDefault;
        _y = kDefault;
    }
    else if (passed == 1)
    {
        _x += kStep;
        _y += kStep;
    }

    outX = _x;
    outY = _y;
}

}  // namespace uo::client::gumps
