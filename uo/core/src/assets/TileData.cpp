// SPDX-License-Identifier: BSD-2-Clause
#include "uo/assets/TileData.h"

#include "uo/io/BinaryReader.h"
#include "uo/io/MappedFile.h"

namespace uo::assets
{

namespace
{
constexpr std::size_t kLandGroups = 512;

constexpr std::size_t landGroupSize(bool isNew)
{
    return 4 + 32 * ((isNew ? 8 : 4) + 2 + 20);
}
constexpr std::size_t staticGroupSize(bool isNew)
{
    return 4 + 32 * ((isNew ? 8 : 4) + 1 + 1 + 4 + 2 + 2 + 2 + 1 + 20);
}
}  // namespace

TileData::Format TileData::detect(std::size_t size)
{
    auto fits = [size](bool isNew) {
        std::size_t land = kLandGroups * landGroupSize(isNew);
        return size > land && (size - land) % staticGroupSize(isNew) == 0;
    };
    bool oldFits = fits(false), newFits = fits(true);
    if (newFits && !oldFits)
        return Format::New;
    return Format::Old;
}

bool TileData::load(const std::string& path, Format format)
{
    io::MappedFile f(path);
    if (!f.isOpen())
        return false;
    return loadFromBytes(f.bytes(), format);
}

bool TileData::loadFromBytes(std::span<const std::uint8_t> bytes, Format format)
{
    if (format == Format::Auto)
        format = detect(bytes.size());
    _format    = format;
    bool isNew = format == Format::New;

    if (bytes.size() < kLandGroups * landGroupSize(isNew))
        return false;

    io::BinaryReader r(bytes);
    _land.clear();
    _statics.clear();
    _land.reserve(kLandGroups * 32);

    auto readFlags = [&] { return isNew ? r.readU64LE() : r.readU32LE(); };

    for (std::size_t g = 0; g < kLandGroups; ++g)
    {
        r.readU32LE();
        for (int j = 0; j < 32; ++j)
        {
            LandTile t;
            t.flags = readFlags();
            t.texId = r.readU16LE();
            t.name  = r.readASCII(20);
            _land.push_back(std::move(t));
        }
    }

    std::size_t groups = r.remaining() / staticGroupSize(isNew);
    _statics.reserve(groups * 32);
    for (std::size_t g = 0; g < groups; ++g)
    {
        r.readU32LE();
        for (int j = 0; j < 32; ++j)
        {
            StaticTile t;
            t.flags      = readFlags();
            t.weight     = r.readU8();
            t.layer      = r.readU8();
            t.count      = r.readI32LE();
            t.animId     = r.readU16LE();
            t.hue        = r.readU16LE();
            t.lightIndex = r.readU16LE();
            t.height     = r.readU8();
            t.name       = r.readASCII(20);
            _statics.push_back(std::move(t));
        }
    }
    return !r.overflowed();
}

}  // namespace uo::assets
