// SPDX-License-Identifier: BSD-2-Clause
// Part of the AxmolUO client. Player actions the client gumps trigger (ClassicUO's GameActions
// as far as gumps use them), and a packet implementation from ClassicUO's OutgoingPackets.
// The world model does not send these; the gumps do, through this interface.
#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace uo::client::gumps
{

class GumpActions
{
public:
    virtual ~GumpActions() = default;

    virtual void doubleClick(uint32_t serial) = 0;                                   // 0x06
    virtual void singleClick(uint32_t serial) = 0;                                   // 0x09
    virtual void pickUp(uint32_t serial, uint16_t amount) = 0;                       // 0x07
    virtual void dropToContainer(uint32_t serial, uint32_t container, int x, int y) = 0;  // 0x08
    virtual void dropToWorld(uint32_t serial, int x, int y, int z) = 0;              // 0x08
    virtual void equip(uint32_t serial, uint8_t layer, uint32_t mobile) = 0;         // 0x13
    virtual void requestStatus(uint32_t serial) = 0;                                 // 0x34 type 4
    virtual void requestSkills(uint32_t serial) = 0;                                 // 0x34 type 5
    virtual void requestProfile(uint32_t serial) = 0;                                // 0xB8
    virtual void useSkill(int index) = 0;                                            // 0x12 0x24
    virtual void setSkillLock(uint16_t index, uint8_t lock) = 0;                     // 0x3A
    virtual void setStatLock(uint8_t stat, uint8_t lock) = 0;                        // 0xBF 0x1A
    virtual void setWarMode(bool on) = 0;                                            // 0x72
    virtual void requestHelp() = 0;                                                  // 0x9B
    virtual void requestQuestMenu(uint32_t player) = 0;                              // 0xD7 0x32
    virtual void rename(uint32_t serial, const std::string& name) = 0;               // 0x75
};

// Builds the packets and hands them to `send` (the session's send function).
class PacketGumpActions final : public GumpActions
{
public:
    explicit PacketGumpActions(std::function<void(std::vector<uint8_t>)> send) : _send(std::move(send)) {}

    void doubleClick(uint32_t serial) override;
    void singleClick(uint32_t serial) override;
    void pickUp(uint32_t serial, uint16_t amount) override;
    void dropToContainer(uint32_t serial, uint32_t container, int x, int y) override;
    void dropToWorld(uint32_t serial, int x, int y, int z) override;
    void equip(uint32_t serial, uint8_t layer, uint32_t mobile) override;
    void requestStatus(uint32_t serial) override;
    void requestSkills(uint32_t serial) override;
    void requestProfile(uint32_t serial) override;
    void useSkill(int index) override;
    void setSkillLock(uint16_t index, uint8_t lock) override;
    void setStatLock(uint8_t stat, uint8_t lock) override;
    void setWarMode(bool on) override;
    void requestHelp() override;
    void requestQuestMenu(uint32_t player) override;
    void rename(uint32_t serial, const std::string& name) override;

private:
    std::function<void(std::vector<uint8_t>)> _send;
};

}  // namespace uo::client::gumps
