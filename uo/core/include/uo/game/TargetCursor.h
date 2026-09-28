// SPDX-License-Identifier: BSD-2-Clause
// Ported from ClassicUO (Game/Managers/TargetManager.cs): the targeting cursor state machine and the
// 0x6C target responses. No engine types: hooks cover everything that touches the world, gumps, or
// the socket. uo/client/input turns clicks into target() calls and Escape into cancel().

#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>

namespace uo::game
{

enum class CursorTarget : int8_t
{
    Invalid = -1,
    Object = 0,
    Position = 1,
    MultiPlacement = 2,
    SetTargetClientSide = 3,
    Grab,
    SetGrabBag,
    HueCommandTarget,
    IgnorePlayerTarget,
    CallbackTarget,
};

enum class TargetType : uint8_t
{
    Neutral,
    Harmful,
    Beneficial,
    Cancel,
};

enum class Notoriety : uint8_t
{
    Unknown = 0,
    Innocent,
    Ally,
    Gray,
    Criminal,
    Enemy,
    Murderer,
    Invulnerable,
};

struct MultiTargetInfo
{
    uint16_t model{0}, xOffset{0}, yOffset{0}, zOffset{0}, hue{0};
};

struct LastTargetInfo
{
    uint32_t serial{0};
    uint16_t graphic{0xFFFF};
    uint16_t x{0xFFFF}, y{0xFFFF};
    int8_t z{INT8_MIN};

    bool isEntity() const { return serial != 0 && serial < 0x80000000u; }
    bool isStatic() const { return !isEntity() && graphic != 0 && graphic != 0xFFFF; }
    bool isLand() const { return !isStatic(); }
};

// An entity the player clicked while targeting.
struct TargetEntity
{
    uint32_t serial{0};
    uint16_t graphic{0};
    uint16_t x{0}, y{0};
    int8_t z{0};
    uint16_t amount{0};
    Notoriety notoriety{Notoriety::Unknown};
    bool isPlayer{false};
};

inline bool isMobileSerial(uint32_t s) { return s > 0 && s < 0x40000000u; }
inline bool isItemSerial(uint32_t s) { return s >= 0x40000000u && s < 0x80000000u; }

struct TargetCursorHooks
{
    std::function<void(std::span<const uint8_t>)> send;
    std::function<void(uint32_t serial)> requestMobileStatus;
    // Show "This may flag you criminal!" and call `proceed` if the player agrees. Return false when
    // a query is already open; the target is then sent without asking.
    std::function<bool(std::function<void()> proceed)> askCriminalAction;
    std::function<void()> clearMultiPreview;       // HouseManager.Remove(0) and custom-house reset
    std::function<void(uint32_t serial, uint16_t amount)> grabItem;
    std::function<void(uint32_t serial)> setGrabBag;
    std::function<void(uint32_t serial)> ignorePlayer;
    std::function<void()> cancelDoubleClick;       // Mouse.CancelDoubleClick = true
};

struct TargetOptions
{
    bool criminalActionQuery{true};             // EnabledCriminalActionQuery
    bool beneficialCriminalActionQuery{false};  // EnabledBeneficialCriminalActionQuery
    bool addSurfaceHeight{true};                // client 7.0.9.0 and later target the top of surfaces
};

class TargetCursor
{
public:
    // A callback cursor gets the clicked object's serial, or a tile via the tile overload, or
    // nothing when cancelled.
    struct Picked
    {
        std::optional<uint32_t> serial;
        std::optional<std::array<int, 4>> tile;  // graphic, x, y, z
    };
    using Callback = std::function<void(const Picked*)>;

    explicit TargetCursor(TargetCursorHooks hooks) : _hooks(std::move(hooks)) {}

    // 0x6C from the server.
    bool handleServerTarget(std::span<const uint8_t> packet);
    // 0x99 from the server.
    bool handleMultiPlacement(std::span<const uint8_t> packet);

    void setTargeting(CursorTarget targeting, uint32_t cursorId, TargetType type);
    void setTargeting(Callback callback, uint32_t cursorId, TargetType type);
    void setTargetingMulti(uint32_t deedSerial, uint16_t model, uint16_t x, uint16_t y, uint16_t z, uint16_t hue);

    void cancel();
    void reset();

    // The player's notoriety decides whether harmful or beneficial targets ask first.
    void target(const TargetEntity& entity, Notoriety playerNotoriety, const TargetOptions& options);
    // A land or static tile. `graphic` 0 is land; `staticSurfaceHeight` is the static's tiledata
    // height when it is a surface, else 0.
    void target(uint16_t graphic, uint16_t x, uint16_t y, int16_t z, uint8_t staticSurfaceHeight,
                const TargetOptions& options);
    void targetLast();
    void sendMultiTarget(uint16_t x, uint16_t y, int8_t z);

    bool isTargeting() const { return _isTargeting; }
    CursorTarget state() const { return _state; }
    TargetType type() const { return _type; }
    uint32_t cursorId() const { return _cursorId; }
    const std::optional<MultiTargetInfo>& multi() const { return _multi; }
    const LastTargetInfo& lastTarget() const { return _last; }

    uint32_t lastAttack{0};
    uint32_t selectedTarget{0};

    static std::array<uint8_t, 19> objectPacket(uint32_t cursorId, TargetType type, uint32_t serial, uint16_t x,
                                                uint16_t y, int16_t z, uint16_t graphic);
    static std::array<uint8_t, 19> positionPacket(uint32_t cursorId, TargetType type, uint16_t x, uint16_t y,
                                                  int16_t z, uint16_t graphic);
    static std::array<uint8_t, 19> cancelPacket(CursorTarget state, uint32_t cursorId, TargetType type);

private:
    void clearWithoutCancelPacket();
    void sendPosition(uint16_t graphic, uint16_t x, uint16_t y, int8_t z);
    void send(std::span<const uint8_t> bytes);

    TargetCursorHooks _hooks;
    Callback _callback;
    CursorTarget _state{CursorTarget::Invalid};
    TargetType _type{TargetType::Neutral};
    uint32_t _cursorId{0};
    bool _isTargeting{false};
    std::optional<MultiTargetInfo> _multi;
    LastTargetInfo _last;
    std::array<uint8_t, 19> _lastPacket{};
};

}  // namespace uo::game
