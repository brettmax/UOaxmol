// SPDX-License-Identifier: BSD-2-Clause
#include "gumps/GumpActions.h"

#include "uo/net/PacketWriter.h"

#include <string>

namespace uo::client::gumps
{

using uo::net::PacketWriter;

void PacketGumpActions::doubleClick(uint32_t serial)
{
    _send(PacketWriter(0x06, 5).u32(serial).finish());
}

void PacketGumpActions::singleClick(uint32_t serial)
{
    _send(PacketWriter(0x09, 5).u32(serial).finish());
}

void PacketGumpActions::pickUp(uint32_t serial, uint16_t amount)
{
    _send(PacketWriter(0x07, 7).u32(serial).u16(amount).finish());
}

void PacketGumpActions::dropToContainer(uint32_t serial, uint32_t container, int x, int y)
{
    // Pre-6.0.1.7 layout (no grid index), which is what a Second Age client sends.
    _send(PacketWriter(0x08, 14)
              .u32(serial)
              .u16(static_cast<uint16_t>(x))
              .u16(static_cast<uint16_t>(y))
              .i8(0)
              .u32(container)
              .finish());
}

void PacketGumpActions::dropToWorld(uint32_t serial, int x, int y, int z)
{
    _send(PacketWriter(0x08, 14)
              .u32(serial)
              .u16(static_cast<uint16_t>(x))
              .u16(static_cast<uint16_t>(y))
              .i8(static_cast<int8_t>(z))
              .u32(0xFFFFFFFF)
              .finish());
}

void PacketGumpActions::equip(uint32_t serial, uint8_t layer, uint32_t mobile)
{
    _send(PacketWriter(0x13, 10).u32(serial).u8(layer).u32(mobile).finish());
}

void PacketGumpActions::requestStatus(uint32_t serial)
{
    _send(PacketWriter(0x34, 10).u32(0xEDEDEDED).u8(0x04).u32(serial).finish());
}

void PacketGumpActions::requestSkills(uint32_t serial)
{
    _send(PacketWriter(0x34, 10).u32(0xEDEDEDED).u8(0x05).u32(serial).finish());
}

void PacketGumpActions::requestProfile(uint32_t serial)
{
    _send(PacketWriter::variable(0xB8).u8(0x00).u32(serial).finish());
}

void PacketGumpActions::useSkill(int index)
{
    _send(PacketWriter::variable(0x12).u8(0x24).ascii(std::to_string(index) + " 0").finish());
}

void PacketGumpActions::setSkillLock(uint16_t index, uint8_t lock)
{
    _send(PacketWriter::variable(0x3A).u16(index).u8(lock).finish());
}

void PacketGumpActions::setStatLock(uint8_t stat, uint8_t lock)
{
    _send(PacketWriter::variable(0xBF).u16(0x1A).u8(stat).u8(lock).finish());
}

void PacketGumpActions::setWarMode(bool on)
{
    _send(PacketWriter(0x72, 5).u8(on ? 1 : 0).u8(0x32).u8(0x00).finish());
}

void PacketGumpActions::requestHelp()
{
    _send(PacketWriter(0x9B, 258).zero(257).finish());
}

void PacketGumpActions::requestQuestMenu(uint32_t player)
{
    _send(PacketWriter::variable(0xD7).u32(player).u16(0x32).u8(0x00).finish());
}

void PacketGumpActions::rename(uint32_t serial, const std::string& name)
{
    _send(PacketWriter(0x75, 35).u32(serial).ascii(name, 30).finish());
}

}  // namespace uo::client::gumps
