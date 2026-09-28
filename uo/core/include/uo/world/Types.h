// SPDX-License-Identifier: BSD-2-Clause
// Ported from ClassicUO (Game/Data, Game/SerialHelper.cs).
#pragma once

#include "uo/io/ClientVersion.h"

#include <cstdint>

namespace uo::world
{

using Serial = uint32_t;

inline constexpr Serial kInvalidSerial = 0xFFFFFFFFu;

constexpr bool isValidSerial(Serial s) noexcept { return s > 0 && s < 0x80000000u; }
constexpr bool isMobileSerial(Serial s) noexcept { return s > 0 && s < 0x40000000u; }
constexpr bool isItemSerial(Serial s) noexcept { return s >= 0x40000000u && s < 0x80000000u; }

using ClientVersion = uo::ClientVersion;

// Version gates used by the handlers. Same packing as uo::cv (io/ClientVersion.h).
namespace versions
{
inline constexpr ClientVersion CV_200    = makeVersion(2, 0, 0, 0);
inline constexpr ClientVersion CV_305D   = makeVersion(3, 0, 5, 'd');
inline constexpr ClientVersion CV_306E   = makeVersion(3, 0, 0, 'e');
inline constexpr ClientVersion CV_308Z   = makeVersion(3, 0, 8, 'z');
inline constexpr ClientVersion CV_500A   = makeVersion(5, 0, 0, 'a');
inline constexpr ClientVersion CV_5090   = makeVersion(5, 0, 9, 0);
inline constexpr ClientVersion CV_6017   = makeVersion(6, 0, 1, 8);
inline constexpr ClientVersion CV_60142  = makeVersion(6, 0, 14, 2);
inline constexpr ClientVersion CV_60143  = makeVersion(6, 0, 14, 3);
inline constexpr ClientVersion CV_7000   = makeVersion(7, 0, 0, 0);
inline constexpr ClientVersion CV_7090   = makeVersion(7, 0, 9, 0);
inline constexpr ClientVersion CV_70331  = makeVersion(7, 0, 33, 1);
inline constexpr ClientVersion CV_706000 = makeVersion(7, 0, 60, 0);
inline constexpr ClientVersion CV_70796  = makeVersion(7, 0, 79, 6);
} // namespace versions

enum class Layer : uint8_t
{
    Invalid = 0x00,
    OneHanded = 0x01,
    TwoHanded = 0x02,
    Shoes = 0x03,
    Pants = 0x04,
    Shirt = 0x05,
    Helmet = 0x06,
    Gloves = 0x07,
    Ring = 0x08,
    Talisman = 0x09,
    Necklace = 0x0A,
    Hair = 0x0B,
    Waist = 0x0C,
    Torso = 0x0D,
    Bracelet = 0x0E,
    Face = 0x0F,
    Beard = 0x10,
    Tunic = 0x11,
    Earrings = 0x12,
    Arms = 0x13,
    Cloak = 0x14,
    Backpack = 0x15,
    Robe = 0x16,
    Skirt = 0x17,
    Legs = 0x18,
    Mount = 0x19,
    ShopBuyRestock = 0x1A,
    ShopBuy = 0x1B,
    ShopSell = 0x1C,
    Bank = 0x1D,
};

enum class Direction : uint8_t
{
    North = 0x00,
    Right = 0x01,
    East = 0x02,
    Down = 0x03,
    South = 0x04,
    Left = 0x05,
    West = 0x06,
    Up = 0x07,
    Mask = 0x07,
    Running = 0x80,
    None = 0xED,
};

constexpr Direction directionMasked(Direction d) noexcept { return Direction(uint8_t(d) & 0x07); }
constexpr bool directionRunning(Direction d) noexcept { return (uint8_t(d) & 0x80) != 0; }

// Direction from (curX, curY) to (newX, newY); None when the points are equal.
Direction calculateDirection(int curX, int curY, int newX, int newY) noexcept;

// Entity status flags (packet byte).
namespace flags
{
inline constexpr uint8_t Frozen = 0x01;
inline constexpr uint8_t Female = 0x02;
inline constexpr uint8_t Poisoned = 0x04; // also "Flying" for 7.0+ clients
inline constexpr uint8_t YellowBar = 0x08;
inline constexpr uint8_t IgnoreMobiles = 0x10;
inline constexpr uint8_t Movable = 0x20;
inline constexpr uint8_t WarMode = 0x40;
inline constexpr uint8_t Hidden = 0x80;
} // namespace flags

enum class Notoriety : uint8_t
{
    Unknown = 0x00,
    Innocent = 0x01,
    Ally = 0x02,
    Gray = 0x03,
    Criminal = 0x04,
    Enemy = 0x05,
    Murderer = 0x06,
    Invulnerable = 0x07,
};

enum class MessageType : uint8_t
{
    Regular = 0,
    System = 1,
    Emote = 2,
    Limit3Spell = 3,
    Label = 6,
    Focus = 7,
    Whisper = 8,
    Yell = 9,
    Spell = 10,
    Guild = 13,
    Alliance = 14,
    Command = 15,
    GmChat = 16,
    Encoded = 0xC0,
    Party = 0xFF, // client-assigned
};

enum class TextType : uint8_t
{
    Client,
    System,
    Object,
    GuildAlly,
};

enum class GraphicEffectType : uint8_t
{
    Moving = 0x00,
    Lightning = 0x01,
    FixedXYZ = 0x02,
    FixedFrom = 0x03,
    ScreenFade = 0x04,
    DragEffect = 0x05, // client-assigned
    Nothing = 0xFF,
};

enum class GraphicEffectBlendMode : uint8_t
{
    Normal = 0x00,
    Multiply = 0x01,
    Screen = 0x02,
    ScreenMore = 0x03,
    ScreenLess = 0x04,
    NormalHalfTransparent = 0x05,
    ShadowBlue = 0x06,
    ScreenRed = 0x07,
};

enum class CursorTarget : int8_t
{
    Invalid = -1,
    Object = 0,
    Position = 1,
    MultiPlacement = 2,
    SetTargetClientSide = 3,
};

enum class TargetType : uint8_t
{
    Neutral,
    Harmful,
    Beneficial,
    Cancel,
};

enum class SkillLock : uint8_t
{
    Up = 0,
    Down = 1,
    Locked = 2,
};

enum class Race : uint8_t
{
    Human = 1,
    Elf = 2,
    Gargoyle = 3,
};

enum class Season : uint8_t
{
    Spring,
    Summer,
    Fall,
    Winter,
    Desolation,
};

enum class WeatherType : uint8_t
{
    Rain = 0,
    StormApproach = 1,
    Snow = 2,
    StormBrewing = 3,
    Invalid0 = 0xFE,
    Invalid1 = 0xFF,
};

enum class CharacterSpeed : uint8_t
{
    Normal,
    FastUnmount,
    CantRun,
    FastUnmountAndCantRun,
};

namespace locked_features
{
inline constexpr uint32_t T2A = 0x00000001;
inline constexpr uint32_t UOR = 0x00000002;
inline constexpr uint32_t UOTD = 0x00000004;
inline constexpr uint32_t LBR = 0x00000008;
inline constexpr uint32_t AOS = 0x00000010;
inline constexpr uint32_t SE = 0x00000040;
inline constexpr uint32_t ML = 0x00000080;
} // namespace locked_features

// Mount/body conversion flags derived from 0xB9 (ClassicUO BodyConvFlags).
namespace body_conv
{
inline constexpr uint8_t Anim1 = 0x01;
inline constexpr uint8_t Anim2 = 0x02;
inline constexpr uint8_t Anim3 = 0x04;
inline constexpr uint8_t Anim4 = 0x08;
} // namespace body_conv

inline constexpr int kMaxStepCount = 5;
inline constexpr uint8_t kMaxViewRange = 24;
inline constexpr uint16_t kCorpseGraphic = 0x2006;

} // namespace uo::world
