// SPDX-License-Identifier: BSD-2-Clause
// Ported from ClassicUO (Game/Managers/SeasonManager.cs).

#include "uo/render/Seasons.h"

#include <charconv>
#include <string>

namespace uo::render
{

namespace
{

struct DefaultEntry
{
    SeasonId season;
    bool land;
    uint16_t original;
    uint16_t replacement;
};

// SeasonManager.CreateDefaultSeasonsFile.
constexpr DefaultEntry kDefaults[] = {
    {SeasonId::Spring, false, 0x0CA7, 0x0C84},
    {SeasonId::Spring, false, 0x0CAC, 0x0C46},
    {SeasonId::Spring, false, 0x0CAD, 0x0C48},
    {SeasonId::Spring, false, 0x0CAE, 0x0CB5},
    {SeasonId::Spring, false, 0x0C4A, 0x0CB5},
    {SeasonId::Spring, false, 0x0CAF, 0x0C4E},
    {SeasonId::Spring, false, 0x0CB0, 0x0C4D},
    {SeasonId::Spring, false, 0x0CB6, 0x0D2B},
    {SeasonId::Spring, false, 0x0D0D, 0x0D2B},
    {SeasonId::Spring, false, 0x0D14, 0x0D2B},
    {SeasonId::Spring, false, 0x0D0C, 0x0D29},
    {SeasonId::Spring, false, 0x0D0E, 0x0CBE},
    {SeasonId::Spring, false, 0x0D0F, 0x0CBF},
    {SeasonId::Spring, false, 0x0D10, 0x0CC0},
    {SeasonId::Spring, false, 0x0D11, 0x0C87},
    {SeasonId::Spring, false, 0x0D12, 0x0C38},
    {SeasonId::Spring, false, 0x0D13, 0x0D2F},
    {SeasonId::Fall, false, 0x0CD1, 0x0CD2},
    {SeasonId::Fall, false, 0x0CD4, 0x0CD5},
    {SeasonId::Fall, false, 0x0CDB, 0x0CDC},
    {SeasonId::Fall, false, 0x0CDE, 0x0CDF},
    {SeasonId::Fall, false, 0x0CE1, 0x0CE2},
    {SeasonId::Fall, false, 0x0CE4, 0x0CE5},
    {SeasonId::Fall, false, 0x0CE7, 0x0CE8},
    {SeasonId::Fall, false, 0x0D95, 0x0D97},
    {SeasonId::Fall, false, 0x0D99, 0x0D9B},
    {SeasonId::Fall, false, 0x0CCE, 0x0CCF},
    {SeasonId::Fall, false, 0x0CE9, 0x0D3F},
    {SeasonId::Fall, false, 0x0C9E, 0x0D3F},
    {SeasonId::Fall, false, 0x0CEA, 0x0D40},
    {SeasonId::Fall, false, 0x0C84, 0x1B22},
    {SeasonId::Fall, false, 0x0CB0, 0x1B22},
    {SeasonId::Fall, false, 0x0C8B, 0x0CC6},
    {SeasonId::Fall, false, 0x0C8C, 0x0CC6},
    {SeasonId::Fall, false, 0x0C8D, 0x0CC6},
    {SeasonId::Fall, false, 0x0C8E, 0x0CC6},
    {SeasonId::Fall, false, 0x0CA7, 0x0C48},
    {SeasonId::Fall, false, 0x0CAC, 0x1B1F},
    {SeasonId::Fall, false, 0x0CAD, 0x1B20},
    {SeasonId::Fall, false, 0x0CAE, 0x1B21},
    {SeasonId::Fall, false, 0x0CAF, 0x0D0D},
    {SeasonId::Fall, false, 0x0CB5, 0x0D10},
    {SeasonId::Fall, false, 0x0CB6, 0x0D2B},
    {SeasonId::Fall, false, 0x0CC7, 0x0C4E},
    {SeasonId::Winter, false, 0x0CA7, 0x0CC6},
    {SeasonId::Winter, false, 0x0CAC, 0x0D3D},
    {SeasonId::Winter, false, 0x0CAD, 0x0D33},
    {SeasonId::Winter, false, 0x0CAE, 0x0D33},
    {SeasonId::Winter, false, 0x0CB5, 0x0D33},
    {SeasonId::Winter, false, 0x0CAF, 0x17CD},
    {SeasonId::Winter, false, 0x0C87, 0x17CD},
    {SeasonId::Winter, false, 0x0C89, 0x17CD},
    {SeasonId::Winter, false, 0x0D16, 0x17CD},
    {SeasonId::Winter, false, 0x0D17, 0x17CD},
    {SeasonId::Winter, false, 0x0D32, 0x17CD},
    {SeasonId::Winter, false, 0x0D33, 0x17CD},
    {SeasonId::Winter, false, 0x0CB0, 0x17CD},
    {SeasonId::Winter, false, 0x0C8E, 0x1B8D},
    {SeasonId::Winter, false, 0x0C99, 0x1B8D},
    {SeasonId::Winter, false, 0x0C46, 0x1B9D},
    {SeasonId::Winter, false, 0x0C49, 0x1B9D},
    {SeasonId::Winter, false, 0x0C45, 0x1B9C},
    {SeasonId::Winter, false, 0x0C48, 0x1B9C},
    {SeasonId::Winter, false, 0x0CBF, 0x1B9C},
    {SeasonId::Winter, false, 0x0C4E, 0x1B9C},
    {SeasonId::Winter, false, 0x0D2B, 0x1B9C},
    {SeasonId::Winter, false, 0x0C85, 0x1B9C},
    {SeasonId::Winter, false, 0x0D15, 0x1B9C},
    {SeasonId::Winter, false, 0x0D29, 0x1B9C},
    {SeasonId::Winter, false, 0x0CB1, 0x17CD},
    {SeasonId::Winter, false, 0x0CB2, 0x17CD},
    {SeasonId::Winter, false, 0x0CB3, 0x17CD},
    {SeasonId::Winter, false, 0x0CB4, 0x17CD},
    {SeasonId::Winter, false, 0x0CB7, 0x17CD},
    {SeasonId::Winter, false, 0x0CC5, 0x17CD},
    {SeasonId::Winter, false, 0x0D0C, 0x17CD},
    {SeasonId::Winter, false, 0x0CB6, 0x17CD},
    {SeasonId::Winter, false, 0x0C37, 0x1B1F},
    {SeasonId::Winter, false, 0x0C38, 0x1B1F},
    {SeasonId::Winter, false, 0x0C47, 0x1B1F},
    {SeasonId::Winter, false, 0x0C4A, 0x1B1F},
    {SeasonId::Winter, false, 0x0C4B, 0x1B1F},
    {SeasonId::Winter, false, 0x0C4D, 0x1B1F},
    {SeasonId::Winter, false, 0x0C8C, 0x1B1F},
    {SeasonId::Winter, false, 0x0D2F, 0x1B1F},
    {SeasonId::Winter, false, 0x0C8D, 0x1B22},
    {SeasonId::Winter, false, 0x0C93, 0x1B22},
    {SeasonId::Winter, false, 0x0C94, 0x1B22},
    {SeasonId::Winter, false, 0x0C98, 0x1B22},
    {SeasonId::Winter, false, 0x0C9F, 0x1B22},
    {SeasonId::Winter, false, 0x0CA0, 0x1B22},
    {SeasonId::Winter, false, 0x0CA1, 0x1B22},
    {SeasonId::Winter, false, 0x0CA2, 0x1B22},
    {SeasonId::Winter, false, 0x0CA3, 0x1BAE},
    {SeasonId::Winter, false, 0x0CA4, 0x1BAE},
    {SeasonId::Winter, false, 0x0D0D, 0x1BAE},
    {SeasonId::Winter, false, 0x0D0E, 0x1BAE},
    {SeasonId::Winter, false, 0x0D10, 0x1BAE},
    {SeasonId::Winter, false, 0x0D12, 0x1BAE},
    {SeasonId::Winter, false, 0x0D13, 0x1BAE},
    {SeasonId::Winter, false, 0x0D18, 0x1BAE},
    {SeasonId::Winter, false, 0x0D19, 0x1BAE},
    {SeasonId::Winter, false, 0x0D2D, 0x1BAE},
    {SeasonId::Winter, false, 0x0CC7, 0x1B20},
    {SeasonId::Winter, false, 0x0C84, 0x1B84},
    {SeasonId::Winter, false, 0x0C8B, 0x1B84},
    {SeasonId::Winter, false, 0x0CE9, 0x0CCA},
    {SeasonId::Winter, false, 0x0C9E, 0x0CCA},
    {SeasonId::Winter, false, 0x33A1, 0x17CD},
    {SeasonId::Winter, false, 0x33A2, 0x17CD},
    {SeasonId::Winter, false, 0x33A3, 0x17CD},
    {SeasonId::Winter, false, 0x33A4, 0x17CD},
    {SeasonId::Winter, false, 0x33A6, 0x17CD},
    {SeasonId::Winter, false, 0x33AB, 0x17CD},
    {SeasonId::Winter, true, 0x00C4, 0x011A},
    {SeasonId::Winter, true, 0x00C5, 0x011B},
    {SeasonId::Winter, true, 0x00C6, 0x011C},
    {SeasonId::Winter, true, 0x00C7, 0x011D},
    {SeasonId::Winter, true, 0x00F8, 0x011A},
    {SeasonId::Winter, true, 0x00F9, 0x011B},
    {SeasonId::Winter, true, 0x00FA, 0x011C},
    {SeasonId::Winter, true, 0x00FB, 0x011D},
    {SeasonId::Winter, true, 0x015D, 0x03A9},
    {SeasonId::Winter, true, 0x015E, 0x03AC},
    {SeasonId::Winter, true, 0x015F, 0x03AA},
    {SeasonId::Winter, true, 0x0160, 0x03AB},
    {SeasonId::Winter, true, 0x00C8, 0x011A},
    {SeasonId::Winter, true, 0x00C9, 0x011B},
    {SeasonId::Winter, true, 0x00CA, 0x011C},
    {SeasonId::Winter, true, 0x00CB, 0x011D},
    {SeasonId::Winter, true, 0x00CC, 0x011A},
    {SeasonId::Winter, true, 0x00CD, 0x011B},
    {SeasonId::Winter, true, 0x00CE, 0x011C},
    {SeasonId::Winter, true, 0x00CF, 0x011D},
    {SeasonId::Winter, true, 0x00D0, 0x011A},
    {SeasonId::Winter, true, 0x00D1, 0x011B},
    {SeasonId::Winter, true, 0x00D2, 0x011C},
    {SeasonId::Winter, true, 0x00D3, 0x011D},
    {SeasonId::Winter, true, 0x00D4, 0x011A},
    {SeasonId::Winter, true, 0x00D5, 0x011B},
    {SeasonId::Winter, true, 0x00D6, 0x011C},
    {SeasonId::Winter, true, 0x00D7, 0x011D},
    {SeasonId::Winter, true, 0x00D8, 0x011A},
    {SeasonId::Winter, true, 0x00D9, 0x011B},
    {SeasonId::Winter, true, 0x00DA, 0x011C},
    {SeasonId::Winter, true, 0x00DB, 0x011D},
    {SeasonId::Winter, true, 0x06A1, 0x011A},
    {SeasonId::Winter, true, 0x06A2, 0x011B},
    {SeasonId::Winter, true, 0x06A3, 0x011C},
    {SeasonId::Winter, true, 0x06A4, 0x011D},
    {SeasonId::Winter, true, 0x06AF, 0x011A},
    {SeasonId::Winter, true, 0x06B0, 0x011B},
    {SeasonId::Winter, true, 0x06B1, 0x011C},
    {SeasonId::Winter, true, 0x06B2, 0x011D},
    {SeasonId::Winter, true, 0x06B3, 0x011A},
    {SeasonId::Winter, true, 0x06B4, 0x011B},
    {SeasonId::Winter, true, 0x06B5, 0x011C},
    {SeasonId::Winter, true, 0x06B6, 0x011D},
    {SeasonId::Winter, true, 0x06B7, 0x011A},
    {SeasonId::Winter, true, 0x06B8, 0x011B},
    {SeasonId::Winter, true, 0x06B9, 0x011C},
    {SeasonId::Winter, true, 0x06BA, 0x011D},
    {SeasonId::Winter, true, 0x06BB, 0x011A},
    {SeasonId::Winter, true, 0x06BC, 0x011B},
    {SeasonId::Winter, true, 0x06BD, 0x011C},
    {SeasonId::Winter, true, 0x06BE, 0x011D},
    {SeasonId::Winter, true, 0x06BF, 0x011A},
    {SeasonId::Winter, true, 0x06C0, 0x011B},
    {SeasonId::Winter, true, 0x06C1, 0x011C},
    {SeasonId::Winter, true, 0x06C2, 0x011D},
    {SeasonId::Winter, true, 0x014C, 0x03A4},
    {SeasonId::Winter, true, 0x014D, 0x03A1},
    {SeasonId::Winter, true, 0x014E, 0x03A2},
    {SeasonId::Winter, true, 0x014F, 0x03A3},
    {SeasonId::Winter, true, 0x0161, 0x038C},
    {SeasonId::Winter, true, 0x0162, 0x038B},
    {SeasonId::Winter, true, 0x0163, 0x0389},
    {SeasonId::Winter, true, 0x0164, 0x038A},
    {SeasonId::Winter, true, 0x0165, 0x0388},
    {SeasonId::Winter, true, 0x0166, 0x0387},
    {SeasonId::Winter, true, 0x0167, 0x0386},
    {SeasonId::Winter, true, 0x0168, 0x0385},
    {SeasonId::Winter, true, 0x0169, 0x0390},
    {SeasonId::Winter, true, 0x016A, 0x038F},
    {SeasonId::Winter, true, 0x016B, 0x038D},
    {SeasonId::Winter, true, 0x016C, 0x038E},
    {SeasonId::Winter, true, 0x0171, 0x0394},
    {SeasonId::Winter, true, 0x0172, 0x0393},
    {SeasonId::Winter, true, 0x0173, 0x0392},
    {SeasonId::Winter, true, 0x0174, 0x0391},
    {SeasonId::Winter, true, 0x0547, 0x0395},
    {SeasonId::Winter, true, 0x0548, 0x0396},
    {SeasonId::Winter, true, 0x0549, 0x0397},
    {SeasonId::Winter, true, 0x054A, 0x0398},
    {SeasonId::Winter, true, 0x054B, 0x0399},
    {SeasonId::Winter, true, 0x054C, 0x039A},
    {SeasonId::Winter, true, 0x054D, 0x039B},
    {SeasonId::Winter, true, 0x054E, 0x039C},
    {SeasonId::Winter, true, 0x054F, 0x039D},
    {SeasonId::Winter, true, 0x0550, 0x039F},
    {SeasonId::Winter, true, 0x0551, 0x03A0},
    {SeasonId::Winter, true, 0x0552, 0x03A2},
    {SeasonId::Winter, true, 0x0553, 0x03A5},
    {SeasonId::Winter, true, 0x0554, 0x03A6},
    {SeasonId::Winter, true, 0x0555, 0x03A7},
    {SeasonId::Winter, true, 0x0556, 0x03A8},
    {SeasonId::Winter, true, 0x0324, 0x03A3},
    {SeasonId::Winter, true, 0x0325, 0x03A1},
    {SeasonId::Winter, true, 0x0326, 0x039E},
    {SeasonId::Winter, true, 0x0327, 0x039D},
    {SeasonId::Winter, true, 0x0328, 0x03A4},
    {SeasonId::Winter, true, 0x0329, 0x03A2},
    {SeasonId::Winter, true, 0x032A, 0x03A0},
    {SeasonId::Winter, true, 0x032B, 0x039F},
    {SeasonId::Winter, true, 0x032C, 0x0397},
    {SeasonId::Winter, true, 0x032D, 0x0398},
    {SeasonId::Winter, true, 0x032E, 0x0395},
    {SeasonId::Winter, true, 0x032F, 0x0399},
    {SeasonId::Winter, true, 0x0003, 0x011A},
    {SeasonId::Winter, true, 0x0004, 0x011B},
    {SeasonId::Winter, true, 0x0005, 0x011C},
    {SeasonId::Winter, true, 0x0006, 0x011D},
    {SeasonId::Winter, true, 0x0079, 0x038E},
    {SeasonId::Winter, true, 0x007A, 0x038D},
    {SeasonId::Winter, true, 0x007B, 0x0390},
    {SeasonId::Winter, true, 0x007C, 0x038F},
    {SeasonId::Winter, true, 0x007D, 0x038A},
    {SeasonId::Winter, true, 0x007E, 0x0389},
    {SeasonId::Winter, true, 0x0082, 0x038C},
    {SeasonId::Winter, true, 0x0083, 0x038B},
    {SeasonId::Winter, true, 0x0085, 0x0388},
    {SeasonId::Winter, true, 0x0086, 0x0388},
    {SeasonId::Winter, true, 0x0087, 0x0387},
    {SeasonId::Winter, true, 0x0088, 0x0387},
    {SeasonId::Winter, true, 0x0089, 0x0386},
    {SeasonId::Winter, true, 0x008A, 0x0386},
    {SeasonId::Winter, true, 0x008B, 0x0385},
    {SeasonId::Winter, true, 0x008C, 0x0385},
    {SeasonId::Winter, true, 0x0367, 0x0395},
    {SeasonId::Winter, true, 0x0368, 0x0396},
    {SeasonId::Winter, true, 0x0369, 0x0397},
    {SeasonId::Winter, true, 0x036A, 0x0398},
    {SeasonId::Winter, true, 0x036B, 0x0399},
    {SeasonId::Winter, true, 0x036C, 0x039A},
    {SeasonId::Winter, true, 0x036D, 0x039B},
    {SeasonId::Winter, true, 0x036E, 0x039C},
    {SeasonId::Winter, true, 0x036F, 0x039D},
    {SeasonId::Winter, true, 0x0370, 0x039E},
    {SeasonId::Winter, true, 0x0371, 0x039F},
    {SeasonId::Winter, true, 0x0372, 0x03A0},
    {SeasonId::Winter, true, 0x0373, 0x03A1},
    {SeasonId::Winter, true, 0x0374, 0x03A2},
    {SeasonId::Winter, true, 0x0375, 0x03A3},
    {SeasonId::Winter, true, 0x0376, 0x03A4},
    {SeasonId::Winter, true, 0x0377, 0x03A5},
    {SeasonId::Winter, true, 0x0378, 0x03A6},
    {SeasonId::Winter, true, 0x0379, 0x03A7},
    {SeasonId::Winter, true, 0x037A, 0x03A8},
    {SeasonId::Winter, true, 0x037B, 0x03A9},
    {SeasonId::Winter, true, 0x037C, 0x03AA},
    {SeasonId::Winter, true, 0x037D, 0x03AB},
    {SeasonId::Winter, true, 0x037E, 0x03AC},
    {SeasonId::Winter, true, 0x016D, 0x0394},
    {SeasonId::Winter, true, 0x016E, 0x0393},
    {SeasonId::Winter, true, 0x016F, 0x0391},
    {SeasonId::Winter, true, 0x0170, 0x0392},
    {SeasonId::Winter, true, 0x00EC, 0x0116},
    {SeasonId::Winter, true, 0x00ED, 0x0117},
    {SeasonId::Winter, true, 0x00EE, 0x0114},
    {SeasonId::Winter, true, 0x00EF, 0x0115},
    {SeasonId::Winter, true, 0x00F0, 0x0131},
    {SeasonId::Winter, true, 0x00F1, 0x012E},
    {SeasonId::Winter, true, 0x00F2, 0x012F},
    {SeasonId::Winter, true, 0x00F3, 0x0130},
    {SeasonId::Winter, true, 0x00F4, 0x0110},
    {SeasonId::Winter, true, 0x00F5, 0x0111},
    {SeasonId::Winter, true, 0x00F6, 0x0112},
    {SeasonId::Winter, true, 0x00F7, 0x0113},
    {SeasonId::Winter, true, 0x0231, 0x010C},
    {SeasonId::Winter, true, 0x0232, 0x010D},
    {SeasonId::Winter, true, 0x0233, 0x010E},
    {SeasonId::Winter, true, 0x0234, 0x010F},
    {SeasonId::Winter, true, 0x0235, 0x0110},
    {SeasonId::Winter, true, 0x0236, 0x0111},
    {SeasonId::Winter, true, 0x0237, 0x0112},
    {SeasonId::Winter, true, 0x0238, 0x0113},
    {SeasonId::Winter, true, 0x0239, 0x0114},
    {SeasonId::Winter, true, 0x023A, 0x0115},
    {SeasonId::Winter, true, 0x023B, 0x0116},
    {SeasonId::Winter, true, 0x023C, 0x0117},
    {SeasonId::Winter, true, 0x023D, 0x0745},
    {SeasonId::Winter, true, 0x023E, 0x0746},
    {SeasonId::Winter, true, 0x023F, 0x0747},
    {SeasonId::Winter, true, 0x0240, 0x0748},
    {SeasonId::Winter, true, 0x0241, 0x0749},
    {SeasonId::Winter, true, 0x0242, 0x074A},
    {SeasonId::Winter, true, 0x0243, 0x074B},
    {SeasonId::Winter, true, 0x06CD, 0x074C},
    {SeasonId::Winter, true, 0x06CE, 0x074D},
    {SeasonId::Winter, true, 0x06CF, 0x074E},
    {SeasonId::Winter, true, 0x06D0, 0x074F},
    {SeasonId::Winter, true, 0x06D1, 0x0750},
    {SeasonId::Winter, true, 0x06D2, 0x0751},
    {SeasonId::Winter, true, 0x06D3, 0x0752},
    {SeasonId::Winter, true, 0x06D4, 0x0753},
    {SeasonId::Winter, true, 0x06D5, 0x0754},
    {SeasonId::Winter, true, 0x06D6, 0x0755},
    {SeasonId::Winter, true, 0x06D7, 0x0756},
    {SeasonId::Winter, true, 0x06D8, 0x0757},
    {SeasonId::Winter, true, 0x06D9, 0x0758},
    {SeasonId::Winter, true, 0x06DA, 0x0759},
    {SeasonId::Winter, true, 0x06DB, 0x075A},
    {SeasonId::Winter, true, 0x06DC, 0x075B},
    {SeasonId::Winter, true, 0x06DD, 0x075C},
    {SeasonId::Winter, true, 0x06DE, 0x011A},
    {SeasonId::Winter, true, 0x06DF, 0x011B},
    {SeasonId::Winter, true, 0x06E0, 0x011C},
    {SeasonId::Winter, true, 0x06E1, 0x011D},
    {SeasonId::Winter, true, 0x001A, 0x017B},
    {SeasonId::Winter, true, 0x001B, 0x017A},
    {SeasonId::Winter, true, 0x001C, 0x0179},
    {SeasonId::Winter, true, 0x001D, 0x017C},
    {SeasonId::Winter, true, 0x001E, 0x017D},
    {SeasonId::Winter, true, 0x001F, 0x017E},
    {SeasonId::Winter, true, 0x0020, 0x017F},
    {SeasonId::Winter, true, 0x0021, 0x0180},
    {SeasonId::Winter, true, 0x0022, 0x0181},
    {SeasonId::Winter, true, 0x0023, 0x0182},
    {SeasonId::Winter, true, 0x0024, 0x0183},
    {SeasonId::Winter, true, 0x0025, 0x0184},
    {SeasonId::Winter, true, 0x0026, 0x0185},
    {SeasonId::Winter, true, 0x0027, 0x0186},
    {SeasonId::Winter, true, 0x0028, 0x0187},
    {SeasonId::Winter, true, 0x0029, 0x0188},
    {SeasonId::Winter, true, 0x002A, 0x0189},
    {SeasonId::Winter, true, 0x002B, 0x018A},
    {SeasonId::Winter, true, 0x002C, 0x0183},
    {SeasonId::Winter, true, 0x002D, 0x0184},
    {SeasonId::Winter, true, 0x002E, 0x017F},
    {SeasonId::Winter, true, 0x002F, 0x017C},
    {SeasonId::Winter, true, 0x0030, 0x017F},
    {SeasonId::Winter, true, 0x0031, 0x017A},
    {SeasonId::Winter, true, 0x0032, 0x017B},
    {SeasonId::Winter, true, 0x008D, 0x017B},
    {SeasonId::Winter, true, 0x008E, 0x0182},
    {SeasonId::Winter, true, 0x008F, 0x0181},
    {SeasonId::Winter, true, 0x0090, 0x0189},
    {SeasonId::Winter, true, 0x0091, 0x017A},
    {SeasonId::Winter, true, 0x0092, 0x0183},
    {SeasonId::Winter, true, 0x0093, 0x0187},
    {SeasonId::Winter, true, 0x0094, 0x0188},
    {SeasonId::Winter, true, 0x0095, 0x0179},
    {SeasonId::Winter, true, 0x0096, 0x017B},
    {SeasonId::Winter, true, 0x0097, 0x017F},
    {SeasonId::Winter, true, 0x0098, 0x017C},
    {SeasonId::Winter, true, 0x0099, 0x0183},
    {SeasonId::Winter, true, 0x009A, 0x0184},
    {SeasonId::Winter, true, 0x009B, 0x0189},
    {SeasonId::Winter, true, 0x009C, 0x0187},
    {SeasonId::Winter, true, 0x009D, 0x0183},
    {SeasonId::Winter, true, 0x009E, 0x0181},
    {SeasonId::Winter, true, 0x009F, 0x0181},
    {SeasonId::Winter, true, 0x00A0, 0x0185},
    {SeasonId::Winter, true, 0x00A1, 0x017B},
    {SeasonId::Winter, true, 0x00A2, 0x0180},
    {SeasonId::Winter, true, 0x00A3, 0x017C},
    {SeasonId::Winter, true, 0x00A4, 0x017B},
    {SeasonId::Winter, true, 0x00A5, 0x017A},
    {SeasonId::Winter, true, 0x00A6, 0x017A},
    {SeasonId::Winter, true, 0x00A7, 0x018A},
    {SeasonId::Winter, true, 0x05F1, 0x011A},
    {SeasonId::Winter, true, 0x05F2, 0x011B},
    {SeasonId::Winter, true, 0x05F3, 0x011C},
    {SeasonId::Winter, true, 0x05F4, 0x011D},
    {SeasonId::Winter, true, 0x05F9, 0x011A},
    {SeasonId::Winter, true, 0x05FA, 0x011B},
    {SeasonId::Winter, true, 0x05FB, 0x011C},
    {SeasonId::Winter, true, 0x05FC, 0x011D},
    {SeasonId::Winter, true, 0x05FD, 0x011A},
    {SeasonId::Winter, true, 0x05FE, 0x011B},
    {SeasonId::Winter, true, 0x05FF, 0x011C},
    {SeasonId::Winter, true, 0x0600, 0x011D},
    {SeasonId::Winter, true, 0x0601, 0x011A},
    {SeasonId::Winter, true, 0x0602, 0x011B},
    {SeasonId::Winter, true, 0x0603, 0x011C},
    {SeasonId::Winter, true, 0x0604, 0x011D},
    {SeasonId::Winter, true, 0x02E5, 0x017B},
    {SeasonId::Winter, true, 0x02E6, 0x0181},
    {SeasonId::Winter, true, 0x02E7, 0x0185},
    {SeasonId::Winter, true, 0x02E8, 0x0189},
    {SeasonId::Winter, true, 0x02E9, 0x017A},
    {SeasonId::Winter, true, 0x02EA, 0x0180},
    {SeasonId::Winter, true, 0x02EB, 0x0184},
    {SeasonId::Winter, true, 0x02EC, 0x0188},
    {SeasonId::Winter, true, 0x02ED, 0x0179},
    {SeasonId::Winter, true, 0x02EE, 0x0181},
    {SeasonId::Winter, true, 0x02EF, 0x017F},
    {SeasonId::Winter, true, 0x02F0, 0x017C},
    {SeasonId::Winter, true, 0x02F1, 0x0187},
    {SeasonId::Winter, true, 0x02F2, 0x0184},
    {SeasonId::Winter, true, 0x02F3, 0x0181},
    {SeasonId::Winter, true, 0x02F4, 0x0180},
    {SeasonId::Winter, true, 0x02F5, 0x0187},
    {SeasonId::Winter, true, 0x02F6, 0x017B},
    {SeasonId::Winter, true, 0x02F7, 0x0189},
    {SeasonId::Winter, true, 0x02F8, 0x017F},
    {SeasonId::Winter, true, 0x02F9, 0x0181},
    {SeasonId::Winter, true, 0x02FA, 0x0187},
    {SeasonId::Winter, true, 0x02FB, 0x0187},
    {SeasonId::Winter, true, 0x02FC, 0x017B},
    {SeasonId::Winter, true, 0x02FD, 0x0180},
    {SeasonId::Winter, true, 0x02FE, 0x0180},
    {SeasonId::Winter, true, 0x02FF, 0x017B},
    {SeasonId::Winter, true, 0x0009, 0x011A},
    {SeasonId::Winter, true, 0x000A, 0x011B},
    {SeasonId::Winter, true, 0x000B, 0x011C},
    {SeasonId::Winter, true, 0x000C, 0x011D},
    {SeasonId::Winter, true, 0x000D, 0x011A},
    {SeasonId::Winter, true, 0x000E, 0x011B},
    {SeasonId::Winter, true, 0x000F, 0x011C},
    {SeasonId::Winter, true, 0x0010, 0x011D},
    {SeasonId::Winter, true, 0x0011, 0x011A},
    {SeasonId::Winter, true, 0x0012, 0x011B},
    {SeasonId::Winter, true, 0x0013, 0x011C},
    {SeasonId::Winter, true, 0x0014, 0x011D},
    {SeasonId::Winter, true, 0x0015, 0x011A},
    {SeasonId::Desolation, false, 0x1B7E, 0x1E34},
    {SeasonId::Desolation, false, 0x0D2B, 0x1B15},
    {SeasonId::Desolation, false, 0x0D11, 0x122B},
    {SeasonId::Desolation, false, 0x0D14, 0x122B},
    {SeasonId::Desolation, false, 0x0D17, 0x122B},
    {SeasonId::Desolation, false, 0x0D16, 0x1B8D},
    {SeasonId::Desolation, false, 0x0CB9, 0x1B8D},
    {SeasonId::Desolation, false, 0x0CBA, 0x1B8D},
    {SeasonId::Desolation, false, 0x0CBB, 0x1B8D},
    {SeasonId::Desolation, false, 0x0CBC, 0x1B8D},
    {SeasonId::Desolation, false, 0x0CBD, 0x1B8D},
    {SeasonId::Desolation, false, 0x0CBE, 0x1B8D},
    {SeasonId::Desolation, false, 0x0CC7, 0x1B0D},
    {SeasonId::Desolation, false, 0x0CE9, 0x0ED7},
    {SeasonId::Desolation, false, 0x0CEA, 0x0D3F},
    {SeasonId::Desolation, false, 0x0D0F, 0x1B1C},
    {SeasonId::Desolation, false, 0x0CB8, 0x1CEA},
    {SeasonId::Desolation, false, 0x0C84, 0x1B84},
    {SeasonId::Desolation, false, 0x0C8B, 0x1B84},
    {SeasonId::Desolation, false, 0x0C9E, 0x1182},
    {SeasonId::Desolation, false, 0x0CAD, 0x1AE1},
    {SeasonId::Desolation, false, 0x0C4C, 0x1B16},
    {SeasonId::Desolation, false, 0x0C8E, 0x1B8D},
    {SeasonId::Desolation, false, 0x0C99, 0x1B8D},
    {SeasonId::Desolation, false, 0x0CAC, 0x1B8D},
    {SeasonId::Desolation, false, 0x0C46, 0x1B9D},
    {SeasonId::Desolation, false, 0x0C49, 0x1B9D},
    {SeasonId::Desolation, false, 0x0CB6, 0x1B9D},
    {SeasonId::Desolation, false, 0x0C45, 0x1B9C},
    {SeasonId::Desolation, false, 0x0C48, 0x1B9C},
    {SeasonId::Desolation, false, 0x0C4E, 0x1B9C},
    {SeasonId::Desolation, false, 0x0C85, 0x1B9C},
    {SeasonId::Desolation, false, 0x0CA7, 0x1B9C},
    {SeasonId::Desolation, false, 0x0CAE, 0x1B9C},
    {SeasonId::Desolation, false, 0x0CAF, 0x1B9C},
    {SeasonId::Desolation, false, 0x0CB5, 0x1B9C},
    {SeasonId::Desolation, false, 0x0D15, 0x1B9C},
    {SeasonId::Desolation, false, 0x0D29, 0x1B9C},
    {SeasonId::Desolation, false, 0x0C37, 0x1BAE},
    {SeasonId::Desolation, false, 0x0C38, 0x1BAE},
    {SeasonId::Desolation, false, 0x0C47, 0x1BAE},
    {SeasonId::Desolation, false, 0x0C4A, 0x1BAE},
    {SeasonId::Desolation, false, 0x0C4B, 0x1BAE},
    {SeasonId::Desolation, false, 0x0C4D, 0x1BAE},
    {SeasonId::Desolation, false, 0x0C8C, 0x1BAE},
    {SeasonId::Desolation, false, 0x0C8D, 0x1BAE},
    {SeasonId::Desolation, false, 0x0C93, 0x1BAE},
    {SeasonId::Desolation, false, 0x0C94, 0x1BAE},
    {SeasonId::Desolation, false, 0x0C98, 0x1BAE},
    {SeasonId::Desolation, false, 0x0C9F, 0x1BAE},
    {SeasonId::Desolation, false, 0x0CA0, 0x1BAE},
    {SeasonId::Desolation, false, 0x0CA1, 0x1BAE},
    {SeasonId::Desolation, false, 0x0CA2, 0x1BAE},
    {SeasonId::Desolation, false, 0x0CA3, 0x1BAE},
    {SeasonId::Desolation, false, 0x0CA4, 0x1BAE},
    {SeasonId::Desolation, false, 0x0CB0, 0x1BAE},
    {SeasonId::Desolation, false, 0x0CB1, 0x1BAE},
    {SeasonId::Desolation, false, 0x0CB2, 0x1BAE},
    {SeasonId::Desolation, false, 0x0CB3, 0x1BAE},
    {SeasonId::Desolation, false, 0x0CB4, 0x1BAE},
    {SeasonId::Desolation, false, 0x0CB7, 0x1BAE},
    {SeasonId::Desolation, false, 0x0CC5, 0x1BAE},
    {SeasonId::Desolation, false, 0x0D0C, 0x1BAE},
    {SeasonId::Desolation, false, 0x0D0D, 0x1BAE},
    {SeasonId::Desolation, false, 0x0D0E, 0x1BAE},
    {SeasonId::Desolation, false, 0x0D10, 0x1BAE},
    {SeasonId::Desolation, false, 0x0D12, 0x1BAE},
    {SeasonId::Desolation, false, 0x0D13, 0x1BAE},
    {SeasonId::Desolation, false, 0x0D18, 0x1BAE},
    {SeasonId::Desolation, false, 0x0D19, 0x1BAE},
    {SeasonId::Desolation, false, 0x0D2D, 0x1BAE},
    {SeasonId::Desolation, false, 0x0D2F, 0x1BAE},
};

std::string_view trim(std::string_view s)
{
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t'))
    {
        s.remove_prefix(1);
    }
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r'))
    {
        s.remove_suffix(1);
    }
    return s;
}

bool parseNumber(std::string_view s, uint16_t& out)
{
    s = trim(s);
    int base = 10;
    if (s.size() > 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X'))
    {
        s.remove_prefix(2);
        base = 16;
    }
    unsigned value = 0;
    auto [end, ec] = std::from_chars(s.data(), s.data() + s.size(), value, base);
    if (ec != std::errc{} || end != s.data() + s.size() || value > 0xFFFF)
    {
        return false;
    }
    out = static_cast<uint16_t>(value);
    return true;
}

bool iequalsPrefix(std::string_view s, std::string_view prefix)
{
    if (s.size() < prefix.size())
    {
        return false;
    }
    for (size_t i = 0; i < prefix.size(); ++i)
    {
        char c = s[i];
        if (c >= 'A' && c <= 'Z')
        {
            c = static_cast<char>(c - 'A' + 'a');
        }
        if (c != prefix[i])
        {
            return false;
        }
    }
    return true;
}

}  // namespace

const SeasonTable& SeasonTable::defaults()
{
    static const SeasonTable table = [] {
        SeasonTable t;
        for (const DefaultEntry& e : kDefaults)
        {
            t.set(e.season, e.land, e.original, e.replacement);
        }
        return t;
    }();
    return table;
}

void SeasonTable::set(SeasonId season, bool land, uint16_t original, uint16_t replacement)
{
    // A zero replacement means "unchanged", as in SeasonManager's arrays.
    if (replacement == 0)
    {
        _map.erase(key(season, land, original));
        return;
    }
    _map[key(season, land, original)] = replacement;
}

void SeasonTable::parse(std::string_view text)
{
    while (!text.empty())
    {
        const size_t nl = text.find('\n');
        std::string_view line = trim(text.substr(0, nl));
        text = nl == std::string_view::npos ? std::string_view{} : text.substr(nl + 1);

        if (line.empty() || line.starts_with('#') || line.starts_with("//"))
        {
            continue;
        }

        std::string_view fields[4];
        size_t count = 0;
        while (count < 4)
        {
            const size_t comma = line.find(',');
            fields[count++] = trim(line.substr(0, comma));
            if (comma == std::string_view::npos)
            {
                break;
            }
            line.remove_prefix(comma + 1);
        }
        if (count < 4)
        {
            continue;
        }

        uint16_t original, replacement;
        if (!parseNumber(fields[2], original) || !parseNumber(fields[3], replacement))
        {
            continue;
        }

        const bool land = !iequalsPrefix(fields[1], "static");

        static constexpr std::pair<std::string_view, SeasonId> kNames[] = {
            {"spring", SeasonId::Spring}, {"summer", SeasonId::Summer},         {"fall", SeasonId::Fall},
            {"winter", SeasonId::Winter}, {"desolation", SeasonId::Desolation},
        };
        for (const auto& [name, id] : kNames)
        {
            if (fields[0].size() == name.size() && iequalsPrefix(fields[0], name))
            {
                set(id, land, original, replacement);
                break;
            }
        }
    }
}

uint16_t SeasonTable::staticGraphic(SeasonId season, uint16_t graphic) const
{
    auto it = _map.find(key(season, false, graphic));
    return it == _map.end() ? graphic : it->second;
}

uint16_t SeasonTable::landGraphic(SeasonId season, uint16_t graphic) const
{
    auto it = _map.find(key(season, true, graphic));
    return it == _map.end() ? graphic : it->second;
}

}  // namespace uo::render
