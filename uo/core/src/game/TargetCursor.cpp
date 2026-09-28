// SPDX-License-Identifier: BSD-2-Clause
// Ported from ClassicUO (Game/Managers/TargetManager.cs, Network/OutgoingPackets.cs target packets,
// PacketHandlers TargetCursor and MultiPlacement).

#include "uo/game/TargetCursor.h"

#include "uo/io/BinaryReader.h"

namespace uo::game
{

namespace
{

std::array<uint8_t, 19> targetPacket(uint8_t kind, uint32_t cursorId, TargetType type, uint32_t serial, uint16_t x,
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

std::array<uint8_t, 19> TargetCursor::objectPacket(uint32_t cursorId, TargetType type, uint32_t serial, uint16_t x,
                                                   uint16_t y, int16_t z, uint16_t graphic)
{
    return targetPacket(0x00, cursorId, type, serial, x, y, z, graphic);
}

std::array<uint8_t, 19> TargetCursor::positionPacket(uint32_t cursorId, TargetType type, uint16_t x, uint16_t y,
                                                     int16_t z, uint16_t graphic)
{
    return targetPacket(0x01, cursorId, type, 0, x, y, z, graphic);
}

std::array<uint8_t, 19> TargetCursor::cancelPacket(CursorTarget state, uint32_t cursorId, TargetType type)
{
    return targetPacket(static_cast<uint8_t>(state), cursorId, type, 0, 0xFFFF, 0xFFFF, 0, 0);
}

bool TargetCursor::handleServerTarget(std::span<const uint8_t> packet)
{
    io::BinaryReader r(packet);
    if (r.readU8() != 0x6C)
    {
        return false;
    }

    const auto state = static_cast<CursorTarget>(static_cast<int8_t>(r.readU8()));
    const uint32_t cursorId = r.readU32BE();
    const auto type = static_cast<TargetType>(r.readU8());
    if (r.overflowed())
    {
        return false;
    }

    setTargeting(state, cursorId, type);
    return true;
}

bool TargetCursor::handleMultiPlacement(std::span<const uint8_t> packet)
{
    io::BinaryReader r(packet);
    if (r.readU8() != 0x99)
    {
        return false;
    }

    r.readU8();  // allow ground
    const uint32_t deed = r.readU32BE();
    r.readU8();  // flags
    r.seek(18);
    const uint16_t model = r.readU16BE();
    const uint16_t x = r.readU16BE();
    const uint16_t y = r.readU16BE();
    const uint16_t z = r.readU16BE();
    if (r.overflowed())
    {
        return false;
    }
    // Pre-7.0.9 clients get the 26-byte packet, which carries no hue.
    const uint16_t hue = r.remaining() >= 2 ? r.readU16BE() : 0;
    if (r.overflowed())
    {
        return false;
    }

    setTargetingMulti(deed, model, x, y, z, hue);
    return true;
}

void TargetCursor::send(std::span<const uint8_t> bytes)
{
    if (_hooks.send)
    {
        _hooks.send(bytes);
    }
}

void TargetCursor::clearWithoutCancelPacket()
{
    if (_state == CursorTarget::MultiPlacement)
    {
        _multi.reset();
        _state = CursorTarget::Object;
        if (_hooks.clearMultiPreview)
        {
            _hooks.clearMultiPreview();
        }
    }

    _isTargeting = false;
}

void TargetCursor::reset()
{
    clearWithoutCancelPacket();
    _callback = nullptr;
    _state = CursorTarget::Object;
    _cursorId = 0;
    _multi.reset();
    _type = TargetType::Neutral;
}

void TargetCursor::setTargeting(Callback callback, uint32_t cursorId, TargetType type)
{
    setTargeting(CursorTarget::CallbackTarget, cursorId, type);
    _callback = std::move(callback);
}

void TargetCursor::setTargeting(CursorTarget targeting, uint32_t cursorId, TargetType type)
{
    if (targeting == CursorTarget::Invalid)
    {
        return;
    }

    const bool wasTargeting = _isTargeting;
    _isTargeting = type < TargetType::Cancel;
    _state = targeting;
    _type = type;

    if (!_isTargeting && wasTargeting)
    {
        // The server cancelled: answer with the cursor that was active, so set the id afterwards.
        cancel();
    }

    _cursorId = cursorId;
}

void TargetCursor::setTargetingMulti(uint32_t deedSerial, uint16_t model, uint16_t x, uint16_t y, uint16_t z,
                                     uint16_t hue)
{
    setTargeting(CursorTarget::MultiPlacement, deedSerial, TargetType::Neutral);
    _multi = MultiTargetInfo{model, x, y, z, hue};
}

void TargetCursor::cancel()
{
    if (_state == CursorTarget::MultiPlacement && _hooks.clearMultiPreview)
    {
        _hooks.clearMultiPreview();
    }

    if (_state == CursorTarget::CallbackTarget && _callback)
    {
        _callback(nullptr);
    }

    if (_isTargeting || _type == TargetType::Cancel)
    {
        const auto packet = cancelPacket(_state, _cursorId, _type);
        send(packet);
        _isTargeting = false;
    }

    reset();
}

void TargetCursor::target(const TargetEntity& entity, Notoriety playerNotoriety, const TargetOptions& options)
{
    if (!_isTargeting)
    {
        return;
    }

    const uint32_t serial = entity.serial;

    switch (_state)
    {
    case CursorTarget::Invalid: return;

    case CursorTarget::MultiPlacement:
    case CursorTarget::Position:
    case CursorTarget::Object:
    case CursorTarget::HueCommandTarget:
    case CursorTarget::SetTargetClientSide:
    {
        if (!entity.isPlayer)
        {
            _last = LastTargetInfo{serial, 0xFFFF, 0xFFFF, 0xFFFF, INT8_MIN};
        }

        const auto packet = objectPacket(_cursorId, _type, serial, entity.x, entity.y, entity.z, entity.graphic);

        if (isMobileSerial(serial) && !entity.isPlayer &&
            (playerNotoriety == Notoriety::Innocent || playerNotoriety == Notoriety::Ally))
        {
            bool query = false;

            if (_type == TargetType::Harmful && options.criminalActionQuery && entity.notoriety == Notoriety::Innocent)
            {
                query = true;
            }
            else if (_type == TargetType::Beneficial && options.beneficialCriminalActionQuery &&
                     (entity.notoriety == Notoriety::Criminal || entity.notoriety == Notoriety::Murderer ||
                      entity.notoriety == Notoriety::Gray))
            {
                query = true;
            }

            // With a query already open, ClassicUO targets straight away.
            if (query && _hooks.askCriminalAction && _hooks.askCriminalAction([this, packet] {
                    send(packet);
                    clearWithoutCancelPacket();
                }))
            {
                return;
            }
        }

        if (_state != CursorTarget::SetTargetClientSide)
        {
            _lastPacket = packet;
            send(packet);
            // ClassicUO asks for the status of a newly targeted mobile, but only after recording it
            // as the last target, so the request only goes out when targeting yourself is filtered.
            if (isMobileSerial(serial) && _last.serial != serial && _hooks.requestMobileStatus)
            {
                _hooks.requestMobileStatus(serial);
            }
        }

        clearWithoutCancelPacket();
        if (_hooks.cancelDoubleClick)
        {
            _hooks.cancelDoubleClick();
        }
        break;
    }

    case CursorTarget::Grab:
        if (isItemSerial(serial) && _hooks.grabItem)
        {
            _hooks.grabItem(serial, entity.amount);
        }
        clearWithoutCancelPacket();
        return;

    case CursorTarget::SetGrabBag:
        if (isItemSerial(serial) && _hooks.setGrabBag)
        {
            _hooks.setGrabBag(serial);
        }
        clearWithoutCancelPacket();
        return;

    case CursorTarget::IgnorePlayerTarget:
        if (_hooks.ignorePlayer)
        {
            _hooks.ignorePlayer(serial);
        }
        cancel();
        return;

    case CursorTarget::CallbackTarget:
    {
        Picked picked;
        picked.serial = serial;
        if (_callback)
        {
            _callback(&picked);
        }
        clearWithoutCancelPacket();
        return;
    }
    }
}

void TargetCursor::target(uint16_t graphic, uint16_t x, uint16_t y, int16_t z, uint8_t staticSurfaceHeight,
                          const TargetOptions& options)
{
    if (!_isTargeting)
    {
        return;
    }

    if (_state == CursorTarget::CallbackTarget)
    {
        Picked picked;
        picked.tile = std::array<int, 4>{graphic, x, y, z};
        if (_callback)
        {
            _callback(&picked);
        }
        clearWithoutCancelPacket();
        return;
    }

    if (graphic == 0)
    {
        if (_state == CursorTarget::Object)
        {
            return;
        }
    }
    else if (options.addSurfaceHeight)
    {
        z += staticSurfaceHeight;
    }

    _last = LastTargetInfo{0, graphic, x, y, static_cast<int8_t>(z)};
    sendPosition(graphic, x, y, static_cast<int8_t>(z));
}

void TargetCursor::sendMultiTarget(uint16_t x, uint16_t y, int8_t z)
{
    sendPosition(0, x, y, z);
    _multi.reset();
}

void TargetCursor::targetLast()
{
    if (!_isTargeting)
    {
        return;
    }

    // Replays the last target's body under the current cursor.
    _lastPacket[0] = 0x6C;
    _lastPacket[1] = static_cast<uint8_t>(_state);
    _lastPacket[2] = static_cast<uint8_t>(_cursorId >> 24);
    _lastPacket[3] = static_cast<uint8_t>(_cursorId >> 16);
    _lastPacket[4] = static_cast<uint8_t>(_cursorId >> 8);
    _lastPacket[5] = static_cast<uint8_t>(_cursorId);
    _lastPacket[6] = static_cast<uint8_t>(_type);
    send(_lastPacket);

    if (_hooks.cancelDoubleClick)
    {
        _hooks.cancelDoubleClick();
    }
    clearWithoutCancelPacket();
}

void TargetCursor::sendPosition(uint16_t graphic, uint16_t x, uint16_t y, int8_t z)
{
    if (!_isTargeting)
    {
        return;
    }

    _lastPacket = positionPacket(_cursorId, _type, x, y, z, graphic);
    send(_lastPacket);

    if (_hooks.cancelDoubleClick)
    {
        _hooks.cancelDoubleClick();
    }
    clearWithoutCancelPacket();
}

}  // namespace uo::game
