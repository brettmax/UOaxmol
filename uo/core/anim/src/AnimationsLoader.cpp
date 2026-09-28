// SPDX-License-Identifier: BSD-2-Clause
// Ported from ClassicUO (ClassicUO.Assets/AnimationsLoader.cs).
#include "uo/anim/AnimationsLoader.h"

#include "uo/assets/Color.h"
#include "uo/io/BinaryReader.h"
#include "uo/io/Bwt.h"
#include "uo/io/DefReader.h"

#include <zlib.h>

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

namespace uo::anim
{

using io::BinaryReader;
using io::DefReader;

namespace
{

constexpr uint32_t ANIM_IDX_BLOCK_SIZE = 12; // u32 position, u32 size, u32 unknown
constexpr uint32_t SPRITE_END          = 0x7FFF7FFF;

// The next n bytes, or an empty span when they are not all there.
std::span<const uint8_t> peek(const BinaryReader& reader, size_t n)
{
    return reader.remaining() >= n ? reader.rest().first(n) : std::span<const uint8_t>{};
}

std::string toLower(std::string_view s)
{
    std::string r(s);
    std::transform(r.begin(), r.end(), r.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return r;
}

std::string_view trim(std::string_view s)
{
    const auto ws = " \t\r\n\v\f";
    const auto b  = s.find_first_not_of(ws);
    if (b == std::string_view::npos)
    {
        return {};
    }
    return s.substr(b, s.find_last_not_of(ws) - b + 1);
}

uint32_t peopleGroupOffset(uint16_t graphic)
{
    // Same int arithmetic as ClassicUO, including the wrap for bodies below 200.
    return static_cast<uint32_t>(((int32_t(graphic) - 400) * 175 + 35000) * int32_t(ANIM_IDX_BLOCK_SIZE));
}

uint32_t highGroupOffset(uint16_t graphic)
{
    return static_cast<uint32_t>(int32_t(graphic) * 110 * int32_t(ANIM_IDX_BLOCK_SIZE));
}

uint32_t lowGroupOffset(uint16_t graphic)
{
    return static_cast<uint32_t>(((int32_t(graphic) - 200) * 65 + 22000) * int32_t(ANIM_IDX_BLOCK_SIZE));
}

uint32_t calculateOffset(uint16_t graphic, AnimationGroupsType type, uint32_t flags, int& groupCount)
{
    uint32_t result = 0;
    groupCount      = 0;

    auto group = AnimationGroups::None;

    switch (type)
    {
    case AnimationGroupsType::Monster:
        if (flags & AnimationFlags::CalculateOffsetByPeopleGroup)
        {
            group = AnimationGroups::People;
        }
        else if (flags & AnimationFlags::CalculateOffsetByLowGroup)
        {
            group = AnimationGroups::Low;
        }
        else
        {
            group = AnimationGroups::High;
        }
        break;

    case AnimationGroupsType::SeaMonster:
        result     = highGroupOffset(graphic);
        groupCount = static_cast<int>(LowAnimationGroup::AnimationCount);
        break;

    case AnimationGroupsType::Animal:
        if (flags & AnimationFlags::CalculateOffsetLowGroupExtended)
        {
            if (flags & AnimationFlags::CalculateOffsetByPeopleGroup)
            {
                group = AnimationGroups::People;
            }
            else if (flags & AnimationFlags::CalculateOffsetByLowGroup)
            {
                group = AnimationGroups::Low;
            }
            else
            {
                group = AnimationGroups::High;
            }
        }
        else
        {
            group = AnimationGroups::Low;
        }
        break;

    default:
        group = AnimationGroups::People;
        break;
    }

    switch (group)
    {
    case AnimationGroups::Low:
        result     = lowGroupOffset(graphic);
        groupCount = static_cast<int>(LowAnimationGroup::AnimationCount);
        break;
    case AnimationGroups::High:
        result     = highGroupOffset(graphic);
        groupCount = static_cast<int>(HighAnimationGroup::AnimationCount);
        break;
    case AnimationGroups::People:
        result     = peopleGroupOffset(graphic);
        groupCount = static_cast<int>(PeopleAnimationGroup::AnimationCount);
        break;
    default: break;
    }

    return result;
}

// Decodes one run-length sprite into `frame`. The reader sits on the frame header.
void readSpriteData(BinaryReader& reader, std::span<const uint8_t> palette, FrameInfo& frame, bool alphaCheck)
{
    frame.centerX = reader.readI16LE();
    frame.centerY = reader.readI16LE();
    frame.width   = reader.readI16LE();
    frame.height  = reader.readI16LE();

    if (frame.width <= 0 || frame.height <= 0 || reader.overflowed())
    {
        frame.width  = 0;
        frame.height = 0;
        frame.pixels.clear();
        return;
    }

    const int w = frame.width;
    const int h = frame.height;
    frame.pixels.assign(size_t(w) * size_t(h), 0);

    auto paletteAt = [&](uint8_t i) -> uint16_t { return uint16_t(palette[i * 2]) | (uint16_t(palette[i * 2 + 1]) << 8); };

    uint32_t header = reader.readU32LE();

    while (header != SPRITE_END && reader.position() < reader.size() && !reader.overflowed())
    {
        const int runLength = static_cast<int>(header & 0x0FFF);

        int x = static_cast<int>((header >> 22) & 0x03FF);
        if (x & 0x0200)
        {
            x |= static_cast<int>(0xFFFFFE00);
        }

        int y = static_cast<int>((header >> 12) & 0x3FF);
        if (y & 0x0200)
        {
            y |= static_cast<int>(0xFFFFFE00);
        }

        x += frame.centerX;
        y += frame.centerY + frame.height;

        const auto run = peek(reader, static_cast<size_t>(runLength));
        if (run.size() != static_cast<size_t>(runLength))
        {
            break;
        }
        reader.skip(static_cast<size_t>(runLength));

        // ClassicUO writes block = y * width + x unchecked; out-of-frame pixels are dropped here.
        const bool rowOk = y >= 0 && y < h;
        for (int k = 0; k < runLength; ++k)
        {
            const int px = x + k;
            if (!rowOk || px < 0 || px >= w)
            {
                continue;
            }

            const uint16_t val = paletteAt(run[k]);
            frame.pixels[size_t(y) * size_t(w) + size_t(px)] = (!alphaCheck || val != 0) ? (assets::color16To32(val) | assets::kOpaque) : 0u;
        }

        header = reader.readU32LE();
    }
}

} // namespace

std::string AnimationsLoader::filePath(const std::string& name) const
{
    std::error_code ec;

    if (_config.resolve)
    {
        auto p = _config.resolve(name);
        return !p.empty() && std::filesystem::is_regular_file(p, ec) ? p : std::string();
    }

    const std::filesystem::path direct = std::filesystem::path(_config.directory) / name;
    if (std::filesystem::is_regular_file(direct, ec))
    {
        return direct.string();
    }

    // Client files ship with inconsistent casing (Bodyconv.def vs bodyconv.def).
    const auto wanted = toLower(name);
    for (const auto& entry : std::filesystem::directory_iterator(_config.directory, ec))
    {
        if (entry.is_regular_file(ec) && toLower(entry.path().filename().string()) == wanted)
        {
            return entry.path().string();
        }
    }
    return {};
}

bool AnimationsLoader::load(const AnimationsConfig& config)
{
    _config = config;

    for (auto& f : _files)
    {
        f.idx.close();
        f.mul.close();
    }
    for (auto& u : _uopFiles)
    {
        u.reset();
    }
    _equipConv.clear();
    _mobTypes.clear();
    _bodyInfos.clear();
    _corpseInfos.clear();
    _bodyConvInfos.clear();
    _uopInfos.clear();
    _groupReplaces[0].clear();
    _groupReplaces[1].clear();
    _verdataAnimBlocks.clear();

    bool any = false;

    for (int i = 0; i < FILE_COUNT; ++i)
    {
        const std::string suffix = i == 0 ? std::string() : std::to_string(i + 1);
        const auto pathMul       = filePath("anim" + suffix + ".mul");
        const auto pathIdx       = filePath("anim" + suffix + ".idx");

        if (!pathMul.empty() && !pathIdx.empty())
        {
            // Both halves or neither: an index without its data is useless.
            if (_files[i].mul.open(pathMul) && _files[i].idx.open(pathIdx))
            {
                any = true;
            }
            else
            {
                _files[i].mul.close();
                _files[i].idx.close();
            }
        }
    }

    if (_config.isUopInstallation)
    {
        bool loadUopSequences = false;

        for (int i = 0; i < FILE_COUNT; ++i)
        {
            const auto pathUop = filePath("AnimationFrame" + std::to_string(i + 1) + ".uop");
            if (pathUop.empty())
            {
                continue;
            }

            // Frames are looked up by body/action hash, not by entry number, so the pattern only
            // feeds UopFile's numbered index and is never used.
            auto uop = std::make_unique<io::UopFile>(pathUop, "build/animationlegacyframe/%06u.bin");
            if (uop->load())
            {
                _uopFiles[i] = std::move(uop);
                loadUopSequences = true;
                any              = true;
            }
        }

        if (loadUopSequences)
        {
            loadUop();
        }
    }

    if (_config.version >= cv::CV_500A)
    {
        loadMobTypes();
    }

    loadGroupReplaces();
    processEquipConvDef();
    processBodyDef("Body.def", _bodyInfos);
    processBodyDef("Corpse.def", _corpseInfos);

    if (_config.verdata && _config.verdata->isOpen())
    {
        for (const auto& patch : _config.verdata->patches())
        {
            if (patch.fileId == assets::Verdata::FILE_ID_ANIM_MUL)
            {
                _verdataAnimBlocks[patch.blockId] = patch;
            }
        }
    }

    return any;
}

void AnimationsLoader::loadMobTypes()
{
    const auto path = filePath("mobtypes.txt");
    if (path.empty())
    {
        return;
    }

    static constexpr std::string_view typeNames[5] = {"monster", "sea_monster", "animal", "human", "equipment"};

    std::ifstream in(path, std::ios::binary);
    std::string raw;

    while (std::getline(in, raw))
    {
        std::string_view line = trim(raw);

        if (line.empty() || line[0] == '#' || !std::isdigit(static_cast<unsigned char>(line[0])))
        {
            continue;
        }

        std::vector<std::string_view> parts;
        size_t i = 0;
        while (i < line.size())
        {
            while (i < line.size() && (line[i] == '\t' || line[i] == ' '))
            {
                ++i;
            }
            size_t j = i;
            while (j < line.size() && line[j] != '\t' && line[j] != ' ')
            {
                ++j;
            }
            if (j > i)
            {
                parts.push_back(line.substr(i, j - i));
            }
            i = j;
        }

        if (parts.size() < 3)
        {
            continue;
        }

        int id = 0;
        if (auto [p, ec] = std::from_chars(parts[0].data(), parts[0].data() + parts[0].size(), id);
            ec != std::errc{} || p != parts[0].data() + parts[0].size())
        {
            continue;
        }

        const std::string testType = toLower(parts[1]);
        std::string_view flagsText = parts[2];

        const auto commentIdx = flagsText.find('#');
        if (commentIdx == 0)
        {
            continue;
        }
        if (commentIdx != std::string_view::npos)
        {
            // ClassicUO keeps Substring(0, commentIdx - 1), dropping the digit before '#'.
            flagsText = flagsText.substr(0, commentIdx - 1);
        }
        flagsText = trim(flagsText);

        uint32_t number = 0;
        if (auto [p, ec] = std::from_chars(flagsText.data(), flagsText.data() + flagsText.size(), number, 16);
            ec != std::errc{} || p != flagsText.data() + flagsText.size())
        {
            continue;
        }

        for (int t = 0; t < 5; ++t)
        {
            if (testType == typeNames[t])
            {
                _mobTypes[id] = MobTypeInfo{static_cast<AnimationGroupsType>(t), 0x80000000u | number};
                break;
            }
        }
    }
}

void AnimationsLoader::loadGroupReplaces()
{
    static const std::string files[2] = {"Anim1.def", "Anim2.def"};

    for (int i = 0; i < 2; ++i)
    {
        const auto path = filePath(files[i]);
        if (path.empty())
        {
            continue;
        }

        auto reader = DefReader::fromFile(path);
        while (reader.next())
        {
            const auto group = static_cast<uint16_t>(reader.readInt());
            if (group == 0xFFFF)
            {
                continue;
            }

            // ClassicUO throws (and abandons the rest of Load) on a malformed group; skip the line instead.
            const auto replace = reader.readGroupInt();
            if (!replace)
            {
                continue;
            }

            _groupReplaces[i].emplace_back(group, static_cast<uint8_t>(*replace));
        }
    }
}

void AnimationsLoader::processEquipConvDef()
{
    if (_config.version < CV_300)
    {
        return;
    }

    const auto path = filePath("Equipconv.def");
    if (path.empty())
    {
        return;
    }

    auto reader = DefReader::fromFile(path, 5);
    while (reader.next())
    {
        const auto body       = static_cast<uint16_t>(reader.readInt());
        const auto graphic    = static_cast<uint16_t>(reader.readInt());
        const auto newGraphic = static_cast<uint16_t>(reader.readInt());
        int gump              = reader.readInt();

        if (gump > 0xFFFF)
        {
            continue;
        }

        if (gump == 0)
        {
            gump = graphic;
        }
        else if (gump == 0xFFFF || gump == -1)
        {
            gump = newGraphic;
        }

        const auto color = static_cast<uint16_t>(reader.readInt());

        _equipConv[body][graphic] = EquipConvData{newGraphic, static_cast<uint16_t>(gump), color};
    }
}

void AnimationsLoader::processBodyConvDef(uint32_t flags)
{
    if (_config.version < CV_300)
    {
        return;
    }

    const auto path = filePath("Bodyconv.def");
    if (path.empty())
    {
        return;
    }

    auto reader = DefReader::fromFile(path);
    while (reader.next())
    {
        const auto index = static_cast<uint16_t>(reader.readInt());

        for (int i = 1; i < reader.partsCount(); ++i)
        {
            const int body = reader.readInt();
            if (body < 0)
            {
                continue;
            }

            // The client may only use these graphics when the server enabled the expansion.
            // For file index >= 3 the client accepts the conversion regardless of flags.
            if (i == 1 && !(flags & BodyConvFlags::Anim1))
            {
                continue;
            }
            if (i == 2 && !(flags & BodyConvFlags::Anim2))
            {
                continue;
            }

            int8_t mountedHeightOffset = 0;
            if (i == 1)
            {
                if (index == 0x00C0 || index == 793)
                {
                    mountedHeightOffset = -9;
                }
            }
            else if (i == 2)
            {
                if (index == 0x0579)
                {
                    mountedHeightOffset = 9;
                }
            }
            else if (i == 4)
            {
                mountedHeightOffset = -9;
                if (index == 0x0115 || index == 0x00C0)
                {
                    mountedHeightOffset = 0;
                }
                else if (index == 0x042D)
                {
                    mountedHeightOffset = 3;
                }
            }

            if (i >= FILE_COUNT || !_files[i].idx.isOpen())
            {
                continue;
            }

            _bodyConvInfos[index] = BodyConvInfo{i, calculateTypeByGraphic(static_cast<uint16_t>(body), i),
                                                 static_cast<uint16_t>(body), mountedHeightOffset};
        }
    }
}

void AnimationsLoader::processBodyDef(const std::string& fileName, std::unordered_map<int, BodyInfo>& target)
{
    if (_config.version < CV_300)
    {
        return;
    }

    const auto path = filePath(fileName);
    if (path.empty())
    {
        return;
    }

    auto reader = DefReader::fromFile(path, 1);
    while (reader.next())
    {
        const int index = reader.readInt();

        if (auto it = target.find(index); it != target.end() && it->second.graphic != 0)
        {
            continue;
        }

        const auto group = reader.readGroup();
        if (!group || group->empty())
        {
            continue;
        }

        const int color = reader.readInt();

        // "Yes, this is actually how this is supposed to work." (ClassicUO)
        const int checkIndex = group->size() >= 3 ? (*group)[2] : (*group)[0];

        target[index] = BodyInfo{static_cast<uint16_t>(checkIndex), static_cast<uint16_t>(color)};
    }
}

void AnimationsLoader::loadUop()
{
    if (_config.version <= CV_60144)
    {
        return;
    }

    const auto path = filePath("AnimationSequence.uop");
    if (path.empty())
    {
        return;
    }

    io::UopFile animSeq(path, "build/animationsequence/%08u.bin");
    if (!animSeq.load())
    {
        return;
    }

    // Layout (credit: @tristran, via ClassicUO):
    //   u32 animId; 12 x u32 unknown; u32 replaces;
    //   replaces x { u32 oldGroup; u32 frameCount; u32 newGroup; 60 bytes we skip }
    // An entry with frameCount 0 means "oldGroup is drawn with newGroup's frames".
    std::vector<uint8_t> inflated;

    for (const auto& entryRef : animSeq.entries())
    {
        const auto* entry = &entryRef;
        if (entry->offset <= 0 || entry->length <= 0)
        {
            continue;
        }

        auto data = animSeq.raw(*entry);
        if (data.empty())
        {
            continue;
        }

        BinaryReader reader(data);

        if (entry->compression >= CompressionType::Zlib)
        {
            inflated.resize(static_cast<size_t>(std::max(entry->decompressed, 0)));
            uLongf destLen = static_cast<uLongf>(inflated.size());
            if (::uncompress(inflated.data(), &destLen, data.data(), static_cast<uLong>(data.size())) != Z_OK)
            {
                continue;
            }
            reader = BinaryReader(std::span<const uint8_t>(inflated.data(), destLen));
        }

        if (reader.remaining() == 0)
        {
            continue;
        }

        const uint32_t animId = reader.readU32LE();
        reader.skip(48);
        const int32_t replaces = reader.readI32LE();

        ReplacedAnimations info;
        for (int j = 0; j < MAX_ACTIONS; ++j)
        {
            info[j] = j;
        }

        if (replaces != 48 && replaces != 68)
        {
            for (int32_t k = 0; k < replaces && !reader.overflowed(); ++k)
            {
                const int32_t oldGroup    = reader.readI32LE();
                const uint32_t frameCount = reader.readU32LE();
                const int32_t newGroup    = reader.readI32LE();

                if (frameCount == 0 && oldGroup >= 0 && oldGroup < MAX_ACTIONS)
                {
                    info[oldGroup] = newGroup;
                }

                reader.skip(60);
            }
        }

        _uopInfos[static_cast<int>(animId)] = info;
    }
}

bool AnimationsLoader::replaceBody(uint16_t& body, uint16_t& hue) const
{
    auto it = _bodyInfos.find(body);
    if (it == _bodyInfos.end())
    {
        return false;
    }

    hue = it->second.hue;
    if (body == it->second.graphic)
    {
        return false;
    }

    body = it->second.graphic;
    return true;
}

bool AnimationsLoader::replaceCorpse(uint16_t& body, uint16_t& hue) const
{
    auto it = _corpseInfos.find(body);
    if (it == _corpseInfos.end())
    {
        return false;
    }

    hue = it->second.hue;
    if (body == it->second.graphic)
    {
        return false;
    }

    body = it->second.graphic;
    return true;
}

bool AnimationsLoader::replaceUopGroup(uint16_t body, uint8_t& group) const
{
    auto it = _uopInfos.find(body);
    if (it == _uopInfos.end())
    {
        return false;
    }

    if (group < MAX_ACTIONS)
    {
        group = static_cast<uint8_t>(it->second[group]);
    }
    return true;
}

const AnimationsLoader::BodyConvInfo* AnimationsLoader::findBodyConv(uint16_t body) const
{
    auto it = _bodyConvInfos.find(body);
    return it == _bodyConvInfos.end() ? nullptr : &it->second;
}

const EquipConvData* AnimationsLoader::findEquipConv(uint16_t body, uint16_t animId) const
{
    auto it = _equipConv.find(body);
    if (it == _equipConv.end())
    {
        return nullptr;
    }
    auto jt = it->second.find(animId);
    return jt == it->second.end() ? nullptr : &jt->second;
}

AnimationsLoader::Indices AnimationsLoader::getIndices(uint16_t body) const
{
    Indices out;

    MobTypeInfo mobInfo;
    if (auto it = _mobTypes.find(body); it != _mobTypes.end())
    {
        mobInfo = it->second;
    }

    out.flags = mobInfo.flags;

    if (mobInfo.flags & AnimationFlags::UseUopAnimation)
    {
        out.type = mobInfo.type != AnimationGroupsType::Unknown ? mobInfo.type : calculateTypeByGraphic(body);

        const auto replaced = _uopInfos.find(body);
        char name[64];

        for (int actionIdx = 0; actionIdx < MAX_ACTIONS; ++actionIdx)
        {
            const int action = replaced != _uopInfos.end() ? replaced->second[actionIdx] : actionIdx;
            std::snprintf(name, sizeof(name), "build/animationlegacyframe/%06d/%02d.bin", int(body), action);
            const uint64_t hash = io::UopFile::hash(name);

            for (int index = 0; index < FILE_COUNT; ++index)
            {
                if (!_uopFiles[index])
                {
                    continue;
                }

                if (const auto* data = _uopFiles[index]->byHash(hash))
                {
                    if (out.directions.empty())
                    {
                        out.directions.resize(MAX_ACTIONS);
                    }

                    out.fileIndex = index;

                    auto& d            = out.directions[actionIdx];
                    d.position         = static_cast<uint32_t>(data->offset);
                    d.size             = static_cast<uint32_t>(data->length);
                    d.uncompressedSize = static_cast<uint32_t>(data->decompressed);
                    d.compressionType  = data->compression;
                    break;
                }
            }
        }

        return out;
    }

    uint16_t graphic = body;

    if (const auto* conv = findBodyConv(body))
    {
        // Bodyconv entries carry ClassicUO's INVALID_HUE (0xFF) sentinel, meaning "no hue";
        // it is not reported as a hue here.
        graphic       = conv->graphic;
        out.fileIndex = conv->fileIndex;

        if (_config.version < cv::CV_500A)
        {
            out.type = conv->type;
        }
    }

    if (out.type == AnimationGroupsType::Unknown)
    {
        out.type = mobInfo.type != AnimationGroupsType::Unknown ? mobInfo.type : calculateTypeByGraphic(graphic, out.fileIndex);
    }

    if (!hasMulFile(out.fileIndex))
    {
        return out;
    }

    const auto& idx          = _files[out.fileIndex].idx;
    int actionCount          = 0;
    const uint32_t offset    = calculateOffset(graphic, out.type, out.flags, actionCount);
    const uint64_t end       = idx.size();
    const uint64_t blockSize = uint64_t(actionCount) * MAX_DIRECTIONS * ANIM_IDX_BLOCK_SIZE;

    if (offset >= end || offset + blockSize > end)
    {
        return out;
    }

    BinaryReader reader(idx.slice(offset, blockSize));
    const size_t size = size_t(actionCount) * MAX_DIRECTIONS;
    out.directions.resize(size);

    for (size_t i = 0; i < size; ++i)
    {
        auto& d            = out.directions[i];
        d.position         = reader.readU32LE();
        d.size             = reader.readU32LE();
        d.uncompressedSize = reader.readU32LE();
        d.compressionType  = CompressionType::None;
    }

    // verdata.mul patches (FileID 6) apply to anim.mul only: the other animation files
    // have their own block numbering.
    if (out.fileIndex == 0)
    {
        applyVerdataPatches(out.directions, offset / ANIM_IDX_BLOCK_SIZE);
    }

    return out;
}

void AnimationsLoader::applyVerdataPatches(std::vector<AnimationDirectionIndex>& directions, uint64_t firstBlockIndex) const
{
    if (_verdataAnimBlocks.empty())
    {
        return;
    }

    for (size_t i = 0; i < directions.size(); ++i)
    {
        const uint64_t blockIndex = firstBlockIndex + i;
        if (blockIndex > 0xFFFFFFFFull)
        {
            continue;
        }

        auto it = _verdataAnimBlocks.find(static_cast<uint32_t>(blockIndex));
        if (it == _verdataAnimBlocks.end())
        {
            continue;
        }

        auto& d = directions[i];
        if (it->second.length == 0)
        {
            // "delete block" marker
            d.position = 0;
            d.size     = 0;
        }
        else
        {
            d.position = it->second.position;
            d.size     = it->second.length;
        }
        d.uncompressedSize = 0;
        d.compressionType  = CompressionType::None;
        d.isVerdata        = true;
    }
}

AnimationGroupsType AnimationsLoader::calculateTypeByGraphic(uint16_t graphic, int fileIndex)
{
    if (fileIndex == 1) // anim2
    {
        return graphic < 200 ? AnimationGroupsType::Monster : AnimationGroupsType::Animal;
    }

    if (fileIndex == 2) // anim3
    {
        return graphic < 300   ? AnimationGroupsType::Animal
               : graphic < 400 ? AnimationGroupsType::Monster
                               : AnimationGroupsType::Human;
    }

    return graphic < 200   ? AnimationGroupsType::Monster
           : graphic < 400 ? AnimationGroupsType::Animal
                           : AnimationGroupsType::Human;
}

AnimationGroups AnimationsLoader::getGroupIndex(uint16_t, AnimationGroupsType animType) const
{
    switch (animType)
    {
    case AnimationGroupsType::Animal: return AnimationGroups::Low;
    case AnimationGroupsType::Monster:
    case AnimationGroupsType::SeaMonster: return AnimationGroups::High;
    case AnimationGroupsType::Human:
    case AnimationGroupsType::Equipment: return AnimationGroups::People;
    default: return AnimationGroups::High;
    }
}

uint8_t AnimationsLoader::getDeathAction(uint16_t, uint32_t animFlags, AnimationGroupsType animType, bool second,
                                         bool isRunning) const
{
    if (animFlags & AnimationFlags::CalculateOffsetByLowGroup)
    {
        animType = AnimationGroupsType::Animal;
    }

    if (animFlags & AnimationFlags::CalculateOffsetLowGroupExtended)
    {
        animType = AnimationGroupsType::Monster;
    }

    switch (animType)
    {
    case AnimationGroupsType::Animal:
        if ((animFlags & AnimationFlags::Use2IfHittedWhileRunning) || (animFlags & AnimationFlags::CanFlying))
        {
            return 2;
        }
        if (animFlags & AnimationFlags::UseUopAnimation)
        {
            return second ? 3 : 2;
        }
        return static_cast<uint8_t>(second ? LowAnimationGroup::Die2 : LowAnimationGroup::Die1);

    case AnimationGroupsType::SeaMonster:
        if (!isRunning)
        {
            return 8;
        }
        [[fallthrough]];

    case AnimationGroupsType::Monster:
        if (animFlags & AnimationFlags::UseUopAnimation)
        {
            return second ? 3 : 2;
        }
        return static_cast<uint8_t>(second ? HighAnimationGroup::Die2 : HighAnimationGroup::Die1);

    case AnimationGroupsType::Human:
    case AnimationGroupsType::Equipment:
        return static_cast<uint8_t>(second ? PeopleAnimationGroup::Die2 : PeopleAnimationGroup::Die1);

    default: return 0;
    }
}

void AnimationsLoader::fixSittingDirection(uint8_t& direction, bool& mirror, int& x, int& y,
                                           const SittingInfoData& data) const
{
    switch (direction)
    {
    case 7:
    case 0:
        if (data.direction1 == -1)
        {
            direction = static_cast<uint8_t>(direction == 7 ? data.direction4 : data.direction2);
        }
        else
        {
            direction = static_cast<uint8_t>(data.direction1);
        }
        break;

    case 1:
    case 2:
        if (data.direction2 == -1)
        {
            direction = static_cast<uint8_t>(direction == 1 ? data.direction1 : data.direction3);
        }
        else
        {
            direction = static_cast<uint8_t>(data.direction2);
        }
        break;

    case 3:
    case 4:
        if (data.direction3 == -1)
        {
            direction = static_cast<uint8_t>(direction == 3 ? data.direction2 : data.direction4);
        }
        else
        {
            direction = static_cast<uint8_t>(data.direction3);
        }
        break;

    case 5:
    case 6:
        if (data.direction4 == -1)
        {
            direction = static_cast<uint8_t>(direction == 5 ? data.direction3 : data.direction1);
        }
        else
        {
            direction = static_cast<uint8_t>(data.direction4);
        }
        break;

    default: break;
    }

    // GetSittingAnimDirection
    switch (direction)
    {
    case 0:
        mirror    = true;
        direction = 3;
        break;
    case 2:
        mirror    = true;
        direction = 1;
        break;
    case 4:
        mirror    = false;
        direction = 1;
        break;
    case 6:
        mirror    = false;
        direction = 3;
        break;
    default: break;
    }

    constexpr int SITTING_OFFSET_X = 8;

    if (mirror)
    {
        if (direction == 3)
        {
            y += 25 + data.mirrorOffsetY;
            x += SITTING_OFFSET_X - 4;
        }
        else
        {
            y += data.offsetY + 9;
        }
    }
    else
    {
        if (direction == 3)
        {
            y += 23 + data.mirrorOffsetY;
            x -= 3;
        }
        else
        {
            y += 10 + data.offsetY;
            x -= SITTING_OFFSET_X + 1;
        }
    }
}

std::vector<FrameInfo> AnimationsLoader::readMulAnimationFrames(int fileIndex, const AnimationDirectionIndex& index,
                                                                bool isVerdata) const
{
    if (!isVerdata && (fileIndex < 0 || fileIndex >= FILE_COUNT))
    {
        return {};
    }

    if (index.position == 0 && index.size == 0)
    {
        return {};
    }

    if (index.position == 0xFFFFFFFFu || index.size == 0xFFFFFFFFu || index.size == 0)
    {
        return {};
    }

    // Blocks that came from a verdata.mul patch are read from verdata.mul itself.
    const io::MappedFile* file = nullptr;
    if (isVerdata)
    {
        file = _config.verdata ? &_config.verdata->file() : nullptr;
    }
    else
    {
        file = &_files[fileIndex].mul;
    }

    if (!file || !file->isOpen())
    {
        return {};
    }

    const auto data = file->slice(index.position, index.size);
    if (data.size() < 512 + 4)
    {
        return {};
    }

    BinaryReader reader(data);
    const auto palette = peek(reader, 512);
    reader.skip(512);

    const size_t dataStart    = reader.position();
    const uint32_t frameCount = reader.readU32LE();

    if (frameCount == 0 || uint64_t(frameCount) * 4 > reader.remaining())
    {
        return {};
    }

    std::vector<uint32_t> frameOffsets(frameCount);
    for (auto& o : frameOffsets)
    {
        o = reader.readU32LE();
    }

    std::vector<FrameInfo> frames(frameCount);
    for (uint32_t i = 0; i < frameCount; ++i)
    {
        reader.seek(dataStart + frameOffsets[i]);
        frames[i].num = static_cast<int>(i);
        readSpriteData(reader, palette, frames[i], false);
    }

    return frames;
}

std::optional<std::pair<std::vector<uint8_t>, bool>> AnimationsLoader::readUopBlock(int fileIndex,
                                                                                     const AnimationDirectionIndex& index) const
{
    if (fileIndex < 0 || fileIndex >= FILE_COUNT || !_uopFiles[fileIndex])
    {
        return std::nullopt;
    }

    if (index.position == 0 && index.size == 0)
    {
        return std::nullopt;
    }

    const auto raw = _uopFiles[fileIndex]->data().slice(index.position, index.size);
    if (raw.empty())
    {
        return std::nullopt;
    }

    if (index.compressionType >= CompressionType::Zlib)
    {
        std::vector<uint8_t> out(index.uncompressedSize);
        uLongf destLen = index.uncompressedSize;
        if (::uncompress(out.data(), &destLen, raw.data(), static_cast<uLong>(raw.size())) != Z_OK)
        {
            return std::nullopt;
        }
        out.resize(destLen);

        if (index.compressionType == CompressionType::ZlibBwt)
        {
            out = io::bwtDecompress(out);
            if (out.empty())
            {
                return std::nullopt;
            }
        }

        return std::make_pair(std::move(out), true);
    }

    return std::make_pair(std::vector<uint8_t>(raw.begin(), raw.end()), false);
}

std::array<std::vector<FrameInfo>, MAX_DIRECTIONS> AnimationsLoader::readUopAnimationFramesAllDirections(
    uint16_t, uint8_t, AnimationGroupsType type, int fileIndex, const AnimationDirectionIndex& index) const
{
    std::array<std::vector<FrameInfo>, MAX_DIRECTIONS> result;

    auto block = readUopBlock(fileIndex, index);
    if (!block)
    {
        return result;
    }

    BinaryReader reader(block->first);
    reader.skip(32);

    const int32_t fc         = reader.readI32LE();
    const uint32_t dataStart = reader.readU32LE();

    if (fc <= 0 || reader.overflowed() || dataStart >= reader.size() ||
        uint64_t(fc) * 16 > reader.size() - dataStart)
    {
        return result;
    }

    reader.seek(dataStart);

    struct UopFrame
    {
        size_t position      = 0; // 0 = missing frame
        uint32_t pixelOffset = 0;
        int frameId          = 0;
        int group            = 0;
    };

    // Frame ids are 1-based and run across all directions. Gaps are missing frames that
    // still take a slot, so the per-direction numbering stays aligned.
    std::vector<UopFrame> list;
    list.reserve(static_cast<size_t>(fc));
    int lastFrameId = 1;

    for (int32_t i = 0; i < fc; ++i)
    {
        UopFrame f;
        f.position = reader.position();
        f.group    = reader.readU16LE();
        f.frameId  = reader.readU16LE();
        reader.readU64LE();
        f.pixelOffset = reader.readU32LE();

        while (f.frameId - lastFrameId > 1)
        {
            ++lastFrameId;
            UopFrame gap;
            gap.frameId = lastFrameId;
            list.push_back(gap);
        }

        list.push_back(f);
        lastFrameId = f.frameId;
    }

    const int maxFrameCount = static_cast<int>(list.size());
    const int perDirection  = static_cast<int>(std::lround(maxFrameCount / double(MAX_DIRECTIONS)));
    // Looks like the min amount of frames is 10 for equipment (ClassicUO).
    const int realFrameCount = type == AnimationGroupsType::Equipment ? std::max(10, perDirection) : perDirection;

    if (realFrameCount <= 0)
    {
        return result;
    }

    for (int d = 0; d < MAX_DIRECTIONS; ++d)
    {
        auto& frames = result[d];
        frames.resize(static_cast<size_t>(realFrameCount));
        // ClassicUO leaves unfilled slots at Num 0, which makes its cache blank frame 0 for
        // short equipment strips; every slot keeps its own index here.
        for (int i = 0; i < realFrameCount; ++i)
        {
            frames[i].num = i;
        }
    }

    for (const auto& frame : list)
    {
        if (frame.frameId <= 0)
        {
            continue;
        }

        const int frameDirection = (frame.frameId - 1) / realFrameCount;
        if (frameDirection >= MAX_DIRECTIONS)
        {
            break;
        }

        const int idx = (frame.frameId - 1) % realFrameCount;
        auto& info    = result[frameDirection][idx];

        info.num     = idx;
        info.centerX = 0;
        info.centerY = 0;
        info.width   = 0;
        info.height  = 0;
        info.pixels.clear();

        if (frame.position == 0)
        {
            continue;
        }

        reader.seek(frame.position + frame.pixelOffset);
        const auto palette = peek(reader, 512);
        if (palette.size() != 512)
        {
            continue;
        }
        reader.skip(512);

        readSpriteData(reader, palette, info, true);
    }

    return result;
}

std::vector<FrameInfo> AnimationsLoader::readUopAnimationFrames(uint16_t animId, uint8_t animGroup, uint8_t direction,
                                                                AnimationGroupsType type, int fileIndex,
                                                                const AnimationDirectionIndex& index) const
{
    if (direction >= MAX_DIRECTIONS)
    {
        return {};
    }
    auto all = readUopAnimationFramesAllDirections(animId, animGroup, type, fileIndex, index);
    return std::move(all[direction]);
}

} // namespace uo::anim
