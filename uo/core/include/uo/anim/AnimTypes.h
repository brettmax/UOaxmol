// SPDX-License-Identifier: BSD-2-Clause
// Part of the AxmolUO client core. Ported from ClassicUO.Assets (AnimationsLoader).
#pragma once

#include "uo/io/ClientVersion.h"
#include "uo/io/UOFile.h"

#include <cstdint>

namespace uo::anim
{

inline constexpr int MAX_ACTIONS    = 80; // gargoyle is like 78
inline constexpr int MAX_DIRECTIONS = 5;

// Versions the animation code branches on that uo/io/ClientVersion.h does not name.
inline constexpr ClientVersion CV_300   = makeVersion(3, 0, 0, 0);
inline constexpr ClientVersion CV_60144 = makeVersion(6, 0, 14, 4);

using io::CompressionType;

enum class AnimationGroups : uint8_t
{
    None = 0,
    Low,
    High,
    People,
};

enum class AnimationGroupsType : uint8_t
{
    Monster = 0,
    SeaMonster,
    Animal,
    Human,
    Equipment,
    Unknown,
};

enum class HighAnimationGroup : uint8_t
{
    Walk = 0, Stand, Die1, Die2, Attack1, Attack2, Attack3, Misc1, Misc2, Misc3, Stumble, SlapGround, Cast,
    GetHit1, Misc4, GetHit2, GetHit3, Fidget1, Fidget2, Fly, Land, DieInFlight,
    AnimationCount,
};

enum class PeopleAnimationGroup : uint8_t
{
    WalkUnarmed = 0, WalkArmed, RunUnarmed, RunArmed, Stand, Fidget1, Fidget2, StandOnehandedAttack,
    StandTwohandedAttack, AttackOnehanded, AttackUnarmed1, AttackUnarmed2, AttackTwohandedDown, AttackTwohandedWide,
    AttackTwohandedJab, WalkWarmode, CastDirected, CastArea, AttackBow, AttackCrossbow, GetHit, Die1, Die2,
    OnmountRideSlow, OnmountRideFast, OnmountStand, OnmountAttack, OnmountAttackBow, OnmountAttackCrossbow,
    OnmountSlapHorse, Turn, AttackUnarmedAndWalk, EmoteBow, EmoteSalute, Fidget3,
    AnimationCount,
};

enum class LowAnimationGroup : uint8_t
{
    Walk = 0, Run, Stand, Eat, Unknown, Attack1, Attack2, Attack3, Die1, Fidget1, Fidget2, LieDown, Die2,
    AnimationCount,
};

namespace AnimationFlags
{
enum : uint32_t
{
    None                            = 0x00000,
    Unknown1                        = 0x00001,
    Use2IfHittedWhileRunning        = 0x00002,
    IdleAt8Frame                    = 0x00004,
    CanFlying                       = 0x00008,
    Unknown10                       = 0x00010,
    CalculateOffsetLowGroupExtended = 0x00020,
    CalculateOffsetByLowGroup       = 0x00040,
    Unknown80                       = 0x00080,
    Unknown100                      = 0x00100,
    Unknown200                      = 0x00200,
    CalculateOffsetByPeopleGroup    = 0x00400,
    Unknown800                      = 0x00800,
    Unknown1000                     = 0x01000,
    Unknown2000                     = 0x02000,
    Unknown4000                     = 0x04000,
    Unknown8000                     = 0x08000,
    UseUopAnimation                 = 0x10000,
    Unknown20000                    = 0x20000,
    Unknown40000                    = 0x40000,
    Unknown80000                    = 0x80000,
    Found                           = 0x80000000,
};
} // namespace AnimationFlags

namespace BodyConvFlags
{
enum : uint32_t
{
    Anim1 = 0x1,
    Anim2 = 0x2,
    Anim3 = 0x4,
    Anim4 = 0x8,
    Anim5 = 0x10,
};
} // namespace BodyConvFlags

// Equipment layers, matching the server's Layer enum.
enum class Layer : uint8_t
{
    Invalid        = 0x00,
    OneHanded      = 0x01,
    TwoHanded      = 0x02,
    Shoes          = 0x03,
    Pants          = 0x04,
    Shirt          = 0x05,
    Helmet         = 0x06,
    Gloves         = 0x07,
    Ring           = 0x08,
    Talisman       = 0x09,
    Necklace       = 0x0A,
    Hair           = 0x0B,
    Waist          = 0x0C,
    Torso          = 0x0D,
    Bracelet       = 0x0E,
    Face           = 0x0F,
    Beard          = 0x10,
    Tunic          = 0x11,
    Earrings       = 0x12,
    Arms           = 0x13,
    Cloak          = 0x14,
    Backpack       = 0x15,
    Robe           = 0x16,
    Skirt          = 0x17,
    Legs           = 0x18,
    Mount          = 0x19,
    ShopBuyRestock = 0x1A,
    ShopBuy        = 0x1B,
    ShopSell       = 0x1C,
    Bank           = 0x1D,
};

inline constexpr int LAYER_COUNT = 0x1E;

// One (action, direction) block location inside an animation file.
struct AnimationDirectionIndex
{
    uint32_t position         = 0;
    uint32_t size             = 0;
    uint32_t uncompressedSize = 0;
    CompressionType compressionType = CompressionType::None;
    // The block comes from a verdata.mul patch (FileID 6) and is not inside anim*.mul.
    bool isVerdata = false;
};

struct EquipConvData
{
    uint16_t graphic = 0;
    uint16_t gump    = 0;
    uint16_t color   = 0;

    friend bool operator==(const EquipConvData&, const EquipConvData&) = default;
};

struct SittingInfoData
{
    uint16_t graphic   = 0;
    int8_t direction1  = 0;
    int8_t direction2  = 0;
    int8_t direction3  = 0;
    int8_t direction4  = 0;
    int8_t offsetY     = 0;
    int8_t mirrorOffsetY = 0;
    bool drawBack      = false;
};

} // namespace uo::anim
