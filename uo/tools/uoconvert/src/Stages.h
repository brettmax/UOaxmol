// SPDX-License-Identifier: BSD-2-Clause
#pragma once

#include "Converter.h"

namespace uoconvert
{

bool runHues(Context& ctx, JsonWriter& m);
bool runTileData(Context& ctx, JsonWriter& m);
bool runArt(Context& ctx, JsonWriter& m);
bool runTexmaps(Context& ctx, JsonWriter& m);
bool runGumps(Context& ctx, JsonWriter& m);
bool runLights(Context& ctx, JsonWriter& m);
bool runMaps(Context& ctx, JsonWriter& m);
bool runMultis(Context& ctx, JsonWriter& m);
bool runAnimData(Context& ctx, JsonWriter& m);
bool runCliloc(Context& ctx, JsonWriter& m);
bool runSounds(Context& ctx, JsonWriter& m);
bool runAnims(Context& ctx, JsonWriter& m);
bool runFonts(Context& ctx, JsonWriter& m);
bool runMusic(Context& ctx, JsonWriter& m);
bool runSpine(Context& ctx, JsonWriter& m);

// "<index> {<a>, <b>} <c>" lines of the client's *.def files, as ClassicUO's DefReader reads them.
struct DefLine
{
    std::vector<long> before;  // plain numbers before the group
    std::vector<long> group;   // the {...} group, empty if none
    std::vector<long> after;   // plain numbers after it
    bool hasGroup = false;
};
std::vector<DefLine> readDef(const std::string& path);

}  // namespace uoconvert
