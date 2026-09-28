// SPDX-License-Identifier: BSD-2-Clause
// Part of the AxmolUO client core. Ported from ClassicUO.Assets (TileDataLoader).
#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace uo::assets
{

enum TileFlag : std::uint64_t
{
    TF_None         = 0,
    TF_Background   = 1ull << 0,
    TF_Weapon       = 1ull << 1,
    TF_Transparent  = 1ull << 2,
    TF_Translucent  = 1ull << 3,
    TF_Wall         = 1ull << 4,
    TF_Damaging     = 1ull << 5,
    TF_Impassable   = 1ull << 6,
    TF_Wet          = 1ull << 7,
    TF_Unknown1     = 1ull << 8,
    TF_Surface      = 1ull << 9,
    TF_Bridge       = 1ull << 10,
    TF_Generic      = 1ull << 11,  // stackable
    TF_Window       = 1ull << 12,
    TF_NoShoot      = 1ull << 13,
    TF_ArticleA     = 1ull << 14,
    TF_ArticleAn    = 1ull << 15,
    TF_Internal     = 1ull << 16,
    TF_Foliage      = 1ull << 17,
    TF_PartialHue   = 1ull << 18,
    TF_NoHouse      = 1ull << 19,
    TF_Map          = 1ull << 20,
    TF_Container    = 1ull << 21,
    TF_Wearable     = 1ull << 22,
    TF_LightSource  = 1ull << 23,
    TF_Animation    = 1ull << 24,
    TF_HoverOver    = 1ull << 25,
    TF_NoDiagonal   = 1ull << 26,
    TF_Armor        = 1ull << 27,
    TF_Roof         = 1ull << 28,
    TF_Door         = 1ull << 29,
    TF_StairBack    = 1ull << 30,
    TF_StairRight   = 1ull << 31,
    TF_AlphaBlend   = 1ull << 32,
    TF_UseNewArt    = 1ull << 33,
    TF_ArtUsed      = 1ull << 34,
    TF_NoShadow     = 1ull << 36,
    TF_PixelBleed   = 1ull << 37,
    TF_PlayAnimOnce = 1ull << 38,
    TF_MultiMovable = 1ull << 40,
};

struct LandTile
{
    std::uint64_t flags = 0;
    std::uint16_t texId = 0;
    std::string name;

    bool is(TileFlag f) const { return (flags & f) != 0; }
};

struct StaticTile
{
    std::uint64_t flags     = 0;
    std::uint8_t weight     = 0;
    std::uint8_t layer      = 0;
    std::int32_t count      = 0;
    std::uint16_t animId    = 0;
    std::uint16_t hue       = 0;
    std::uint16_t lightIndex = 0;
    std::uint8_t height     = 0;
    std::string name;

    bool is(TileFlag f) const { return (flags & f) != 0; }
};

class TileData
{
public:
    // Pre-High Seas files use 32-bit flags; 7.0.9.0+ use 64-bit. The Second Age data is old.
    enum class Format
    {
        Auto,
        Old,
        New,
    };

    bool load(const std::string& path, Format format = Format::Auto);
    bool loadFromBytes(std::span<const std::uint8_t> bytes, Format format = Format::Auto);

    // Picks the layout whose record sizes divide the file evenly.
    static Format detect(std::size_t fileSize);

    const std::vector<LandTile>& land() const { return _land; }
    const std::vector<StaticTile>& statics() const { return _statics; }

    const LandTile* landTile(std::uint32_t id) const { return id < _land.size() ? &_land[id] : nullptr; }
    const StaticTile* staticTile(std::uint32_t id) const { return id < _statics.size() ? &_statics[id] : nullptr; }

    Format format() const { return _format; }

private:
    std::vector<LandTile> _land;
    std::vector<StaticTile> _statics;
    Format _format = Format::Auto;
};

}  // namespace uo::assets
