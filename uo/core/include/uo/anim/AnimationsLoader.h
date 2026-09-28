// SPDX-License-Identifier: BSD-2-Clause
// Part of the AxmolUO client core. Ported from ClassicUO.Assets (AnimationsLoader).
//
// Reads mobile, mount and equipment animations from the classic MUL files
// (anim.idx/anim.mul .. anim5.idx/anim5.mul, plus verdata.mul patches) and from the
// UOP containers (AnimationFrame1..N.uop, AnimationSequence.uop), and the text tables
// that remap bodies (mobtypes.txt, Body.def, Bodyconv.def, Corpse.def, Equipconv.def,
// Anim1.def, Anim2.def). Engine-free: decoded frames are RGBA8888 pixel buffers.
#pragma once

#include "uo/anim/AnimTypes.h"
#include "uo/assets/Verdata.h"
#include "uo/io/MappedFile.h"
#include "uo/io/UOFile.h"

#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <optional>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace uo::anim
{

// One decoded animation frame. Pixels are RGBA8888 (R in the lowest byte), row-major,
// width * height entries; a zero pixel is transparent.
struct FrameInfo
{
    int num         = 0;
    int16_t centerX = 0;
    int16_t centerY = 0;
    int16_t width   = 0;
    int16_t height  = 0;
    std::vector<uint32_t> pixels;

    bool empty() const { return width <= 0 || height <= 0; }
};

struct AnimationsConfig
{
    // Folder holding the client files. Lookups are case-insensitive.
    std::string directory;
    ClientVersion version = cv::CV_7000;
    // ClassicUO's FileManager.IsUOPInstallation: only then are AnimationFrame*.uop read.
    bool isUopInstallation = true;
    // Optional verdata.mul, owned by the caller. FileID 6 entries patch anim.mul.
    const assets::Verdata* verdata = nullptr;
    // Optional override for mapping a client file name to a path (for instance
    // assets::Installation::path). The default is a case-insensitive lookup in `directory`.
    std::function<std::string(const std::string&)> resolve;
};

class AnimationsLoader
{
public:
    static constexpr int FILE_COUNT = 10;

    bool load(const AnimationsConfig& config);

    ClientVersion version() const { return _config.version; }

    // Body.def / Corpse.def: swap a body for another one (and hue). Returns true when
    // the body changed.
    bool replaceBody(uint16_t& body, uint16_t& hue) const;
    bool replaceCorpse(uint16_t& body, uint16_t& hue) const;

    // AnimationSequence.uop: map an action to the group that actually holds its frames.
    bool replaceUopGroup(uint16_t body, uint8_t& group) const;

    struct Indices
    {
        std::vector<AnimationDirectionIndex> directions; // actions * MAX_DIRECTIONS for MUL, MAX_ACTIONS for UOP
        uint32_t flags           = AnimationFlags::None;
        int fileIndex            = 0;
        AnimationGroupsType type = AnimationGroupsType::Unknown;
        uint16_t hue             = 0;
    };

    // Where every action/direction of `body` lives. Mirrors AnimationsLoader.GetIndices.
    Indices getIndices(uint16_t body) const;

    // Bodyconv.def depends on the expansions the server enabled (packet 0xB9), so it is
    // applied separately, after login.
    void processBodyConvDef(uint32_t bodyConvFlags);

    AnimationGroups getGroupIndex(uint16_t graphic, AnimationGroupsType animType) const;
    uint8_t getDeathAction(uint16_t animId, uint32_t animFlags, AnimationGroupsType animType, bool second,
                           bool isRunning = false) const;

    // Chair handling: turn the mobile to one of the seat's allowed directions and offset it.
    void fixSittingDirection(uint8_t& direction, bool& mirror, int& x, int& y, const SittingInfoData& data) const;

    // Frames of one direction of an MUL block.
    std::vector<FrameInfo> readMulAnimationFrames(int fileIndex, const AnimationDirectionIndex& index,
                                                  bool isVerdata = false) const;

    // Frames of one direction of a UOP action.
    std::vector<FrameInfo> readUopAnimationFrames(uint16_t animId, uint8_t animGroup, uint8_t direction,
                                                  AnimationGroupsType type, int fileIndex,
                                                  const AnimationDirectionIndex& index) const;

    // All MAX_DIRECTIONS directions of a UOP action from one decompression. Same frames as
    // five readUopAnimationFrames calls.
    std::array<std::vector<FrameInfo>, MAX_DIRECTIONS> readUopAnimationFramesAllDirections(
        uint16_t animId, uint8_t animGroup, AnimationGroupsType type, int fileIndex,
        const AnimationDirectionIndex& index) const;

    using EquipConvMap = std::unordered_map<uint16_t, std::unordered_map<uint16_t, EquipConvData>>;
    const EquipConvMap& equipConversions() const { return _equipConv; }
    const EquipConvData* findEquipConv(uint16_t body, uint16_t animId) const;

    // Anim1.def (index 0, low group) and Anim2.def (index 1, people group): (group, replacement).
    const std::array<std::vector<std::pair<uint16_t, uint8_t>>, 2>& groupReplaces() const { return _groupReplaces; }

    struct BodyConvInfo
    {
        int fileIndex            = 0;
        AnimationGroupsType type = AnimationGroupsType::Unknown;
        uint16_t graphic         = 0;
        int8_t mountHeight       = 0;
    };
    const BodyConvInfo* findBodyConv(uint16_t body) const;

    bool hasMulFile(int index) const { return index >= 0 && index < FILE_COUNT && _files[index].idx.isOpen() && _files[index].mul.isOpen(); }
    bool hasUopFile(int index) const { return index >= 0 && index < FILE_COUNT && _uopFiles[index] != nullptr; }

    static AnimationGroupsType calculateTypeByGraphic(uint16_t graphic, int fileIndex = 0);

private:
    struct MulPair
    {
        io::MappedFile idx;
        io::MappedFile mul;
    };

    struct MobTypeInfo
    {
        AnimationGroupsType type = AnimationGroupsType::Unknown;
        uint32_t flags           = AnimationFlags::None;
    };

    struct BodyInfo
    {
        uint16_t graphic = 0;
        uint16_t hue     = 0;
    };

    using ReplacedAnimations = std::array<int32_t, MAX_ACTIONS>;

    std::string filePath(const std::string& name) const;
    void loadMobTypes();
    void loadGroupReplaces();
    void processEquipConvDef();
    void processBodyDef(const std::string& fileName, std::unordered_map<int, BodyInfo>& target);
    void loadUop();
    void applyVerdataPatches(std::vector<AnimationDirectionIndex>& directions, uint64_t firstBlockIndex) const;
    std::optional<std::pair<std::vector<uint8_t>, bool>> readUopBlock(int fileIndex,
                                                                      const AnimationDirectionIndex& index) const;

    AnimationsConfig _config;
    std::array<MulPair, FILE_COUNT> _files;
    std::array<std::unique_ptr<io::UopFile>, FILE_COUNT> _uopFiles;

    EquipConvMap _equipConv;
    std::unordered_map<int, MobTypeInfo> _mobTypes;
    std::unordered_map<int, BodyInfo> _bodyInfos;
    std::unordered_map<int, BodyInfo> _corpseInfos;
    std::unordered_map<int, BodyConvInfo> _bodyConvInfos;
    std::unordered_map<int, ReplacedAnimations> _uopInfos;
    std::array<std::vector<std::pair<uint16_t, uint8_t>>, 2> _groupReplaces;

    // verdata.mul FileID 6 patches keyed by anim.mul block number.
    std::unordered_map<uint32_t, assets::VerdataPatch> _verdataAnimBlocks;
};

} // namespace uo::anim
