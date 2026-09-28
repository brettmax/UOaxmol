// SPDX-License-Identifier: BSD-2-Clause
// Ported from ClassicUO (Game/Managers/TargetManager.cs): what happens when the player clicks
// while a target cursor is up. The cursor itself (state, type, cursor id, multi preview) lives in
// World::target, which the 0x6C and 0x99 handlers keep; this class answers it with 0x6C
// responses and adds the client-side cursors ClassicUO layers on top (grab, grab bag, ignore
// player, callback).

#pragma once

#include "uo/world/Types.h"

#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>

namespace uo::world
{

class PacketHandlers;
class World;

struct LastTargetInfo
{
    Serial serial{0};
    uint16_t graphic{0xFFFF};
    uint16_t x{0xFFFF}, y{0xFFFF};
    int8_t z{INT8_MIN};

    bool isEntity() const noexcept { return isValidSerial(serial); }
    bool isStatic() const noexcept { return !isEntity() && graphic != 0 && graphic != 0xFFFF; }
    bool isLand() const noexcept { return !isStatic(); }
};

struct TargetingHooks
{
    std::function<void(std::span<const uint8_t>)> send;
    // Show "This may flag you criminal!" and call `proceed` if the player agrees. Return false when
    // a query is already open; the target is then sent without asking.
    std::function<bool(std::function<void()> proceed)> askCriminalAction;
    std::function<void()> clearMultiPreview;  // HouseManager.Remove(0) and custom-house reset
    std::function<void(Serial serial, uint16_t amount)> grabItem;
    std::function<void(Serial serial)> setGrabBag;
    std::function<void(Serial serial)> ignorePlayer;
    std::function<void()> cancelDoubleClick;  // Mouse.CancelDoubleClick = true
};

struct TargetingOptions
{
    bool criminalActionQuery{true};             // EnabledCriminalActionQuery
    bool beneficialCriminalActionQuery{false};  // EnabledBeneficialCriminalActionQuery
};

class Targeting
{
public:
    // Client-side cursors that never came from the server.
    enum class ClientCursor : uint8_t
    {
        None,
        Grab,
        SetGrabBag,
        IgnorePlayer,
        Callback,
    };

    // What a callback cursor picked; null when it was cancelled.
    struct Picked
    {
        std::optional<Serial> serial;
        std::optional<std::array<int, 4>> tile;  // graphic, x, y, z
    };
    using Callback = std::function<void(const Picked*)>;

    Targeting(World& world, TargetingHooks hooks) : _world(world), _hooks(std::move(hooks)) {}

    // Takes over 0x6C so a server cancel is answered with the cursor that was active, as ClassicUO
    // does; the state still lands in World::target.
    void install(PacketHandlers& handlers);

    bool isTargeting() const noexcept;
    ClientCursor clientCursor() const noexcept { return _client; }

    // Starts a client-side cursor.
    void setClientCursor(ClientCursor cursor, TargetType type = TargetType::Neutral);
    void setCallback(Callback callback, TargetType type = TargetType::Neutral);

    // Escape or a right click: tells the server the cursor was cancelled.
    void cancel();

    // Clicked an entity.
    void target(Serial serial);
    // Clicked land (`graphic` 0) or a static. `surfaceHeight` is the static's tiledata height when
    // it is a surface, which clients from 7.0.9.0 add to z.
    void target(uint16_t graphic, uint16_t x, uint16_t y, int16_t z, uint8_t surfaceHeight = 0);
    void targetLast();
    void sendMultiTarget(uint16_t x, uint16_t y, int8_t z);

    const LastTargetInfo& lastTarget() const noexcept { return _last; }
    TargetingOptions options;

    static std::array<uint8_t, 19> objectPacket(uint32_t cursorId, TargetType type, Serial serial, uint16_t x,
                                                uint16_t y, int16_t z, uint16_t graphic);
    static std::array<uint8_t, 19> positionPacket(uint32_t cursorId, TargetType type, uint16_t x, uint16_t y,
                                                  int16_t z, uint16_t graphic);
    static std::array<uint8_t, 19> cancelPacket(CursorTarget state, uint32_t cursorId, TargetType type);

private:
    void serverCursor(CursorTarget state, uint32_t cursorId, TargetType type);
    void clearWithoutCancelPacket();
    void reset();
    void sendPosition(uint16_t graphic, uint16_t x, uint16_t y, int8_t z);
    void send(std::span<const uint8_t> bytes);
    void cancelDoubleClick();

    World& _world;
    TargetingHooks _hooks;
    ClientCursor _client{ClientCursor::None};
    TargetType _clientType{TargetType::Neutral};
    Callback _callback;
    LastTargetInfo _last;
    std::array<uint8_t, 19> _lastPacket{};
};

}  // namespace uo::world
