// SPDX-License-Identifier: BSD-2-Clause
// The English speech.mul keywords that ModernUO's speech handlers test, for installs without
// speech.mul (the UO Renaissance discs ship none). Ids and phrases follow the handlers' own
// comments (Banker, VendorAI, PlayerVendor, BaseAI, HouseRegion, BaseBoat, ...) and the
// standard list's wording as UOX3 documents it (CResponse.h). Each entry uses speech.mul's
// pattern rules: a leading '*' lets the phrase end the line, a trailing '*' lets it start the
// line, both let it stand anywhere, and none means the line is the phrase.

#include "uo/chat/SpeechKeywords.h"

#include <utility>

namespace uo::chat
{

namespace
{

struct Builtin
{
    std::uint16_t id;
    const char* text;
};

// clang-format off
constexpr Builtin kBuiltin[] = {
    // Bankers
    {0x0000, "*withdraw*"},
    {0x0001, "*balance*"},
    {0x0001, "*statement*"},
    {0x0002, "*bank*"},
    {0x0003, "*check*"},

    // Guildmasters
    {0x0004, "*join*"},
    {0x0004, "*member*"},
    {0x0005, "*resign*"},
    {0x0005, "*quit*"},

    // Guarded regions
    {0x0007, "*guards*"},

    // Animal trainers
    {0x0008, "*stable*"},
    {0x0009, "*claim*"},

    // Escorts and the thieves' guildmaster
    {0x001D, "*destination*"},
    {0x001E, "*i will take thee*"},
    {0x001F, "*disguise*"},

    // Shield guards
    {0x0021, "*order shield*"},
    {0x0022, "*chaos shield*"},

    // Houses
    {0x0023, "*i wish to lock this down*"},
    {0x0024, "*i wish to release this*"},
    {0x0025, "*i wish to secure this*"},
    {0x0026, "*i wish to unsecure this*"},
    {0x0027, "*i wish to place a strongbox*"},
    {0x0028, "*i wish to place a trash barrel*"},
    {0x0033, "*remove thyself*"},
    {0x0034, "*i ban thee*"},

    // Players
    {0x002A, "*i resign from my guild*"},
    {0x0030, "*news*"},
    {0x0032, "*i must consider my sins*"},
    {0x0035, "*i renounce my young player status*"},
    {0x0038, "*appraise*"},

    // Hirelings
    {0x003B, "*servant*"},
    {0x0162, "*hire*"},

    // Vendors
    {0x003C, "*vendor buy*"},
    {0x003C, "*vendor purchase*"},
    {0x003D, "*vendor browse*"},
    {0x003D, "*vendor view*"},
    {0x003D, "*vendor look*"},
    {0x003E, "*vendor collect*"},
    {0x003E, "*vendor gold*"},
    {0x003E, "*vendor get*"},
    {0x003F, "*vendor status*"},
    {0x003F, "*vendor info*"},
    {0x0040, "*vendor dismiss*"},
    {0x0040, "*vendor replace*"},
    {0x0041, "*vendor cycle*"},
    {0x014D, "*vendor sell*"},
    {0x0171, "*buy*"},
    {0x0171, "*purchase*"},
    {0x0172, "*browse*"},
    {0x0172, "*view*"},
    {0x0173, "*collect*"},
    {0x0174, "*status*"},
    {0x0175, "*dismiss*"},
    {0x0176, "*cycle*"},
    {0x0177, "*sell*"},

    // Tillermen: the whole line is the order
    {0x0042, "set name*"},
    {0x0043, "remove name"},
    {0x0044, "name"},
    {0x0045, "forward"},
    {0x0046, "backward"},
    {0x0046, "backwards"},
    {0x0046, "back"},
    {0x0047, "left"},
    {0x0047, "drift left"},
    {0x0048, "right"},
    {0x0048, "drift right"},
    {0x0049, "starboard"},
    {0x004A, "port"},
    {0x004B, "forward left"},
    {0x004C, "forward right"},
    {0x004D, "back left"},
    {0x004D, "backward left"},
    {0x004D, "backwards left"},
    {0x004E, "back right"},
    {0x004E, "backward right"},
    {0x004E, "backwards right"},
    {0x004F, "stop"},
    {0x0050, "slow left"},
    {0x0051, "slow right"},
    {0x0052, "slow forward"},
    {0x0053, "slow back"},
    {0x0053, "slow backward"},
    {0x0053, "slow backwards"},
    {0x0054, "slow forward left"},
    {0x0055, "slow forward right"},
    {0x0056, "slow back right"},
    {0x0056, "slow backward right"},
    {0x0056, "slow backwards right"},
    {0x0057, "slow back left"},
    {0x0057, "slow backward left"},
    {0x0057, "slow backwards left"},
    {0x0058, "one left"},
    {0x0058, "left one"},
    {0x0059, "one right"},
    {0x0059, "right one"},
    {0x005A, "one forward"},
    {0x005A, "forward one"},
    {0x005B, "one back"},
    {0x005B, "one backward"},
    {0x005B, "one backwards"},
    {0x005B, "back one"},
    {0x005B, "backward one"},
    {0x005B, "backwards one"},
    {0x005C, "one forward left"},
    {0x005C, "forward left one"},
    {0x005D, "one forward right"},
    {0x005D, "forward right one"},
    {0x005E, "one back right"},
    {0x005E, "one backward right"},
    {0x005E, "one backwards right"},
    {0x005E, "back one right"},
    {0x005E, "backward one right"},
    {0x005E, "backwards one right"},
    {0x005F, "one back left"},
    {0x005F, "one backward left"},
    {0x005F, "one backwards left"},
    {0x005F, "back one left"},
    {0x005F, "backward one left"},
    {0x005F, "backwards one left"},
    {0x0060, "nav"},
    {0x0061, "start"},
    {0x0062, "continue"},
    {0x0063, "goto*"},
    {0x0064, "single*"},
    {0x0065, "turn right"},
    {0x0066, "turn left"},
    {0x0067, "turn around"},
    {0x0067, "come about"},
    {0x0068, "unfurl sail"},
    {0x0069, "furl sail"},
    {0x006A, "drop anchor"},
    {0x006A, "lower anchor"},
    {0x006B, "raise anchor"},
    {0x006B, "lift anchor"},
    {0x006B, "hoist anchor"},

    // Townsfolk
    {0x006C, "*train*"},
    {0x006C, "*teach*"},
    {0x009D, "*move*"},
    {0x009E, "*time*"},

    // Factions
    {0x00E4, "*i wish to access the city treasury*"},
    {0x00E5, "*i wish to resign as finance minister*"},
    {0x00E6, "*orders*"},
    {0x00E9, "*what is my faction term status*"},
    {0x00EA, "*message faction*"},
    {0x00EC, "*showscore*"},
    {0x00ED, "*i am sheriff*"},
    {0x00EE, "*i wish to resign as sheriff*"},
    {0x00EF, "*you are fired*"},
    {0x0178, "*i honor your leadership*"},

    // Pets: "<name> <order>" ends the line with the order, "all <order>" is the whole line
    {0x0155, "*come"},
    {0x0156, "*drop"},
    {0x0157, "*fetch"},
    {0x0158, "*get"},
    {0x0159, "*bring"},
    {0x015A, "*follow"},
    {0x015B, "*friend"},
    {0x015C, "*guard"},
    {0x015C, "*guard me"},
    {0x015D, "*kill"},
    {0x015E, "*attack"},
    {0x0161, "*stop"},
    {0x0163, "*follow me"},
    {0x0164, "all come"},
    {0x0165, "all follow"},
    {0x0166, "all guard"},
    {0x0167, "all stop"},
    {0x0168, "all kill"},
    {0x0169, "all attack"},
    {0x016B, "all guard me"},
    {0x016C, "all follow me"},
    {0x016D, "*release"},
    {0x016E, "*transfer"},
    {0x016F, "*stay"},
    {0x0170, "all stay"},
};
// clang-format on

}  // namespace

void SpeechKeywords::loadBuiltin()
{
    _entries.clear();
    for (const auto& [id, text] : kBuiltin)
    {
        add(id, text);
    }
}

}  // namespace uo::chat
