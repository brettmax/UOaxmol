// SPDX-License-Identifier: BSD-2-Clause
// Ported from ClassicUO (Game/Managers/TargetManager.cs, Network/OutgoingPackets.cs target packets,
// PacketHandlers.TargetCursor).

#include "uo/world/Targeting.h"

#include "uo/io/BinaryReader.h"
#include "uo/world/PacketHandlers.h"
#include "uo/world/World.h"

namespace uo::world
{

namespace
{

std::array<uint8_t, 19> targetPacket(uint8_t kind, uint32_t cursorId, TargetType type, Serial serial, uint16_t x,
                                     uint16_t y, int16_t z, uint16_t graphic)
{
    const auto uz = static_cast<uint16_t>(z);
    return {0x6C,
            kind,
            static_cast<uint8_t>(cursorId >> 24),
            static_cast<uint8_t>(cursorId >> 16),
            static_cast<uint8_t>(cursorId >> 8),
            static_cast<uint8_t>(cursorId),
            static_cast<uint8_t>(type),
            static_cast<uint8_t>(serial >> 24),
            static_cast<uint8_t>(serial >> 16),
            static_cast<uint8_t>(serial >> 8),
            static_cast<uint8_t>(serial),
            static_cast<uint8_t>(x >> 8),
            static_cast<uint8_t>(x),
            static_cast<uint8_t>(y >> 8),
            static_cast<uint8_t>(y),
            static_cast<uint8_t>(uz >> 8),
            static_cast<uint8_t>(uz),
            static_cast<uint8_t>(graphic >> 8),
            static_cast<uint8_t>(graphic)};
}

}  // namespace

std::array<uint8_t, 19> Targeting::objectPacket(uint32_t cursorId, TargetType type, Serial serial, uint16_t x,
                                                uint16_t y, int16_t z, uint16_t graphic)
{
    return targetPacket(0x00, cursorId, type, serial, x, y, z, graphic);
}

std::array<uint8_t, 19> Targeting::positionPacket(uint32_t cursorId, TargetType type, uint16_t x, uint16_t y,
                                                  int16_t z, uint16_t graphic)
{
    return targetPacket(0x01, cursorId, type, 0, x, y, z, graphic);
}

std::array<uint8_t, 19> Targeting::cancelPacket(CursorTarget state, uint32_t cursorId, TargetType type)
{
    return targetPacket(static_cast<uint8_t>(state), cursorId, type, 0, 0xFFFF, 0xFFFF, 0, 0);
}

void Targeting::install(PacketHandlers& handlers)
{
    handlers.setHook(0x6C, [this](World&, io::BinaryReader& r) {
        const auto state = static_cast<CursorTarget>(static_cast<int8_t>(r.readU8()));
        const uint32_t cursorId = r.readU32BE();
        const auto type = static_cast<TargetType>(r.readU8());
        if (!r.overflowed())
        {
            serverCursor(state, cursorId, type);
        }
    });
}

void Targeting::serverCursor(CursorTarget state, uint32_t cursorId, TargetType type)
{
    if (state == CursorTarget::Invalid)
    {
        return;
    }

    // A server cursor replaces any client-side one.
    if (_client == ClientCursor::Callback && _callback)
    {
        _callback(nullptr);
    }
    _client = ClientCursor::None;
    _callback = nullptr;

    TargetState& t = _world.target;
    const bool wasTargeting = t.isTargeting;
    const uint32_t previousCursor = t.cursorId;
    const bool wasMulti = t.state == CursorTarget::MultiPlacement;

    _world.setTargeting(state, cursorId, type);

    // The server cancelled the active cursor: echo the cancel with that cursor's id.
    if (wasTargeting && !t.isTargeting)
    {
        if (wasMulti && _hooks.clearMultiPreview)
        {
            _hooks.clearMultiPreview();
        }
        send(cancelPacket(state, previousCursor, type));
    }
}

bool Targeting::isTargeting() const noexcept
{
    return _world.target.isTargeting;
}

void Targeting::setClientCursor(ClientCursor cursor, TargetType type)
{
    if (cursor == ClientCursor::None)
    {
        cancel();
        return;
    }

    _client = cursor;
    _clientType = type;
    TargetState& t = _world.target;
    t.isTargeting = true;
    t.state = CursorTarget::SetTargetClientSide;
    t.type = type;
    _world.listener().onTargetCursorChanged();
}

void Targeting::setCallback(Callback callback, TargetType type)
{
    setClientCursor(ClientCursor::Callback, type);
    _callback = std::move(callback);
}

void Targeting::send(std::span<const uint8_t> bytes)
{
    if (_hooks.send)
    {
        _hooks.send(bytes);
    }
}

void Targeting::cancelDoubleClick()
{
    if (_hooks.cancelDoubleClick)
    {
        _hooks.cancelDoubleClick();
    }
}

void Targeting::clearWithoutCancelPacket()
{
    TargetState& t = _world.target;
    if (t.state == CursorTarget::MultiPlacement)
    {
        t.multi.reset();
        t.state = CursorTarget::Object;
        if (_hooks.clearMultiPreview)
        {
            _hooks.clearMultiPreview();
        }
    }

    t.isTargeting = false;
    _client = ClientCursor::None;
    _callback = nullptr;
    _world.listener().onTargetCursorChanged();
}

void Targeting::reset()
{
    clearWithoutCancelPacket();
    TargetState& t = _world.target;
    t.state = CursorTarget::Object;
    t.cursorId = 0;
    t.multi.reset();
    t.type = TargetType::Neutral;
}

void Targeting::cancel()
{
    TargetState& t = _world.target;

    if (_client != ClientCursor::None)
    {
        // Client-side cursors never reached the server, so there is nothing to tell it.
        // (ClassicUO sends a 0x6C cancel for these too.)
        if (_client == ClientCursor::Callback && _callback)
        {
            _callback(nullptr);
        }
        reset();
        return;
    }

    if (t.state == CursorTarget::MultiPlacement && _hooks.clearMultiPreview)
    {
        _hooks.clearMultiPreview();
    }

    if (t.isTargeting || t.type == TargetType::Cancel)
    {
        send(cancelPacket(t.state, t.cursorId, t.type));
        t.isTargeting = false;
    }

    reset();
}

void Targeting::target(Serial serial)
{
    TargetState& t = _world.target;
    if (!t.isTargeting)
    {
        return;
    }

    Entity* entity = _world.inGame() ? _world.get(serial) : nullptr;
    if (entity == nullptr)
    {
        return;
    }

    const Player* player = _world.player();
    const bool isPlayer = _world.isPlayer(serial);

    switch (_client)
    {
    case ClientCursor::Grab:
        if (isItemSerial(serial) && _hooks.grabItem)
        {
            _hooks.grabItem(serial, static_cast<Item*>(entity)->amount);
        }
        clearWithoutCancelPacket();
        return;

    case ClientCursor::SetGrabBag:
        if (isItemSerial(serial) && _hooks.setGrabBag)
        {
            _hooks.setGrabBag(serial);
        }
        clearWithoutCancelPacket();
        return;

    case ClientCursor::IgnorePlayer:
        if (_hooks.ignorePlayer)
        {
            _hooks.ignorePlayer(serial);
        }
        cancel();
        return;

    case ClientCursor::Callback:
    {
        Callback callback = std::move(_callback);
        clearWithoutCancelPacket();
        if (callback)
        {
            Picked picked;
            picked.serial = serial;
            callback(&picked);
        }
        return;
    }

    case ClientCursor::None: break;
    }

    if (t.state == CursorTarget::Invalid)
    {
        return;
    }

    if (!isPlayer)
    {
        _last = LastTargetInfo{serial, 0xFFFF, 0xFFFF, 0xFFFF, INT8_MIN};
    }

    const auto packet = objectPacket(t.cursorId, t.type, serial, entity->x, entity->y, entity->z, entity->graphic);

    if (entity->isMobile() && !isPlayer && player != nullptr &&
        (player->notoriety == Notoriety::Innocent || player->notoriety == Notoriety::Ally))
    {
        const Notoriety noto = static_cast<Mobile*>(entity)->notoriety;
        const bool query =
            (t.type == TargetType::Harmful && options.criminalActionQuery && noto == Notoriety::Innocent) ||
            (t.type == TargetType::Beneficial && options.beneficialCriminalActionQuery &&
             (noto == Notoriety::Criminal || noto == Notoriety::Murderer || noto == Notoriety::Gray));

        // With a query already open, ClassicUO targets straight away.
        if (query && _hooks.askCriminalAction && _hooks.askCriminalAction([this, packet] {
                send(packet);
                clearWithoutCancelPacket();
            }))
        {
            return;
        }
    }

    if (t.state != CursorTarget::SetTargetClientSide)
    {
        _lastPacket = packet;
        send(packet);
        // ClassicUO asks for the status of a newly targeted mobile, but records it as the last
        // target first, so this only fires for the player.
        if (entity->isMobile() && _last.serial != serial)
        {
            _world.requestMobileStatus(serial);
        }
    }

    clearWithoutCancelPacket();
    cancelDoubleClick();
}

void Targeting::target(uint16_t graphic, uint16_t x, uint16_t y, int16_t z, uint8_t surfaceHeight)
{
    TargetState& t = _world.target;
    if (!t.isTargeting)
    {
        return;
    }

    if (_client == ClientCursor::Callback)
    {
        Callback callback = std::move(_callback);
        clearWithoutCancelPacket();
        if (callback)
        {
            Picked picked;
            picked.tile = std::array<int, 4>{graphic, x, y, z};
            callback(&picked);
        }
        return;
    }

    if (_client != ClientCursor::None)
    {
        return;  // grab, grab bag and ignore take entities only
    }

    if (graphic == 0)
    {
        if (t.state == CursorTarget::Object)
        {
            return;
        }
    }
    else if (_world.clientVersion >= makeVersion(7, 0, 9, 0))
    {
        z += surfaceHeight;
    }

    _last = LastTargetInfo{0, graphic, x, y, static_cast<int8_t>(z)};
    sendPosition(graphic, x, y, static_cast<int8_t>(z));
}

void Targeting::sendMultiTarget(uint16_t x, uint16_t y, int8_t z)
{
    sendPosition(0, x, y, z);
    _world.target.multi.reset();
}

void Targeting::targetLast()
{
    const TargetState& t = _world.target;
    if (!t.isTargeting || _client != ClientCursor::None)
    {
        return;
    }

    // Replays the last target's body under the current cursor.
    _lastPacket[0] = 0x6C;
    _lastPacket[1] = static_cast<uint8_t>(t.state);
    _lastPacket[2] = static_cast<uint8_t>(t.cursorId >> 24);
    _lastPacket[3] = static_cast<uint8_t>(t.cursorId >> 16);
    _lastPacket[4] = static_cast<uint8_t>(t.cursorId >> 8);
    _lastPacket[5] = static_cast<uint8_t>(t.cursorId);
    _lastPacket[6] = static_cast<uint8_t>(t.type);
    send(_lastPacket);

    cancelDoubleClick();
    clearWithoutCancelPacket();
}

void Targeting::sendPosition(uint16_t graphic, uint16_t x, uint16_t y, int8_t z)
{
    const TargetState& t = _world.target;
    if (!t.isTargeting)
    {
        return;
    }

    _lastPacket = positionPacket(t.cursorId, t.type, x, y, z, graphic);
    send(_lastPacket);

    cancelDoubleClick();
    clearWithoutCancelPacket();
}

}  // namespace uo::world
