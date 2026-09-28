// SPDX-License-Identifier: BSD-2-Clause
// Ported from ClassicUO (Game/GameObjects/Entity.cs, Item.cs, Mobile.cs, PlayerMobile.cs).
//
// Engine-free entity state. Rendering, animation playback and walking live in
// other modules; this is the data the packet handlers maintain.
#pragma once

#include "uo/world/Types.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <deque>
#include <map>
#include <string>
#include <vector>

namespace uo::world
{

enum class HitsRequest : uint8_t
{
    None,
    Pending,
    Received,
};

enum class EntityKind : uint8_t
{
    Item,
    Mobile,
};

class Entity
{
public:
    Entity(EntityKind kind, Serial serial) : serial(serial), _kind(kind) {}
    virtual ~Entity() = default;

    Entity(const Entity&) = delete;
    Entity& operator=(const Entity&) = delete;

    EntityKind kind() const noexcept { return _kind; }
    bool isItem() const noexcept { return _kind == EntityKind::Item; }
    bool isMobile() const noexcept { return _kind == EntityKind::Mobile; }

    bool isHidden() const noexcept { return (flagBits & flags::Hidden) != 0; }

    // Hue normalisation from Entity.FixHue: clamps out-of-range hues to 1 and keeps the
    // partial-hue / translucency bits.
    void fixHue(uint16_t hue) noexcept;

    // Child items (container contents or equipment), in the order the server sent them.
    const std::vector<Serial>& contents() const noexcept { return _contents; }
    bool isEmpty() const noexcept { return _contents.empty(); }

    Serial serial;
    uint16_t graphic{0};
    uint16_t hue{0};
    uint16_t x{0};
    uint16_t y{0};
    int8_t z{0};
    Direction direction{Direction::North};
    uint8_t flagBits{0};
    uint16_t hits{0};
    uint16_t hitsMax{0};
    HitsRequest hitsRequest{HitsRequest::None};
    bool isClicked{false};
    std::string name;

private:
    friend class World;
    EntityKind _kind;
    std::vector<Serial> _contents;
};

class Item final : public Entity
{
public:
    explicit Item(Serial serial) : Entity(EntityKind::Item, serial) {}

    bool onGround() const noexcept { return !isValidSerial(container); }
    bool isCorpse() const noexcept { return graphic == kCorpseGraphic; }
    bool isCoin() const noexcept { return graphic == 0x0EEA || graphic == 0x0EED || graphic == 0x0EF0; }

    uint16_t amount{0};
    Serial container{kInvalidSerial};
    Layer layer{Layer::Invalid};
    uint8_t lightId{0};
    bool isMulti{false};
    bool isDamageable{false};
    bool wantUpdateMulti{true};
    bool opened{false};
    uint32_t price{0};
};

struct MobileStep
{
    int x{0};
    int y{0};
    int8_t z{0};
    Direction direction{Direction::North};
    bool run{false};
};

// Last animation the server asked this mobile to play (0x6E, 0xE2, 0xBF 0x19/0x2B).
// The animation module resolves the actual group from the body's animation data.
struct ServerAnimation
{
    enum class Source : uint8_t
    {
        None,
        Legacy,     // 0x6E: action is an old-style action id
        New,        // 0xE2: type/action/mode triple
        Death,      // 0xAF: play the death action
        FrameSet,   // 0xBF 0x19 v5 / 0x2B: freeze on a given frame
    };

    Source source{Source::None};
    uint16_t action{0};
    uint16_t type{0};
    uint8_t mode{0};
    uint8_t delay{0};
    uint16_t frameCount{0};
    uint16_t repeatCount{0};
    bool forward{true};
    bool repeat{false};
    bool running{false};
    uint32_t sequence{0}; // bumps on every request so observers can detect repeats
};

class Mobile : public Entity
{
public:
    explicit Mobile(Serial serial) : Entity(EntityKind::Mobile, serial) {}

    bool isDead() const noexcept;
    void setDead(bool value) noexcept { _isDead = value; }
    bool isParalyzed() const noexcept { return (flagBits & flags::Frozen) != 0; }
    bool isYellowHits() const noexcept { return (flagBits & flags::YellowBar) != 0; }
    bool ignoreMobiles() const noexcept { return (flagBits & flags::IgnoreMobiles) != 0; }
    // SA clients (7.0+) signal poison through 0x16/0x17 and use the flag bit for flying.
    bool isPoisoned(ClientVersion version) const noexcept;
    bool isFlying(ClientVersion version) const noexcept;
    void setSAPoison(bool value) noexcept { _saPoisoned = value; }
    virtual bool inWarMode() const noexcept { return (flagBits & flags::WarMode) != 0; }
    bool isHuman() const noexcept;

    // Remote mobiles: queue a step towards (x, y, z, dir). Returns false when the queue is full,
    // in which case the caller should snap the mobile to the target position.
    bool enqueueStep(int x, int y, int8_t z, Direction dir, bool run);
    void clearSteps() noexcept { steps.clear(); }
    void endPosition(int& x, int& y, int8_t& z, Direction& dir) const noexcept;

    uint16_t stamina{0};
    uint16_t staminaMax{0};
    uint16_t mana{0};
    uint16_t manaMax{0};
    Notoriety notoriety{Notoriety::Unknown};
    Race race{Race::Human};
    CharacterSpeed speedMode{CharacterSpeed::Normal};
    bool isFemale{false};
    bool isRenamable{false};
    bool isRunning{false};
    std::string title;
    std::deque<MobileStep> steps;
    ServerAnimation animation;

private:
    bool _isDead{false};
    bool _saPoisoned{false};
};

struct Skill
{
    std::string name;
    bool hasButton{false};
    uint16_t valueFixed{0};
    uint16_t baseFixed{0};
    uint16_t capFixed{1000};
    SkillLock lock{SkillLock::Up};

    float value() const noexcept { return valueFixed / 10.0f; }
    float base() const noexcept { return baseFixed / 10.0f; }
    float cap() const noexcept { return capFixed / 10.0f; }
};

struct BuffIcon
{
    uint16_t type{0};     // BuffIconType value from the packet
    uint16_t iconId{0};   // index into the buff table
    uint16_t timerSeconds{0}; // 0 means no expiry
    uint32_t titleCliloc{0};
    uint32_t descriptionCliloc{0};
    uint32_t extraCliloc{0};
    std::string titleArgs;
    std::string descriptionArgs;
    std::string extraArgs;
    std::string text; // translated tooltip, when a cliloc resolver is attached
};

class Player final : public Mobile
{
public:
    explicit Player(Serial serial) : Mobile(serial) {}

    bool inWarMode() const noexcept override { return warMode; }

    bool warMode{false};

    uint16_t strength{0};
    uint16_t dexterity{0};
    uint16_t intelligence{0};
    SkillLock strLock{SkillLock::Up};
    SkillLock dexLock{SkillLock::Up};
    SkillLock intLock{SkillLock::Up};
    uint32_t gold{0};
    uint16_t weight{0};
    uint16_t weightMax{0};
    int16_t statsCap{0};
    uint8_t followers{0};
    uint8_t followersMax{0};
    uint16_t luck{0};
    uint32_t tithingPoints{0};
    int16_t damageMin{0};
    int16_t damageMax{0};

    int16_t physicalResistance{0};
    int16_t fireResistance{0};
    int16_t coldResistance{0};
    int16_t poisonResistance{0};
    int16_t energyResistance{0};
    int16_t maxPhysicalResistance{0};
    int16_t maxFireResistance{0};
    int16_t maxColdResistance{0};
    int16_t maxPoisonResistance{0};
    int16_t maxEnergyResistance{0};
    int16_t defenseChanceIncrease{0};
    int16_t maxDefenseChanceIncrease{0};
    int16_t hitChanceIncrease{0};
    int16_t swingSpeedIncrease{0};
    int16_t damageIncrease{0};
    int16_t lowerReagentCost{0};
    int16_t spellDamageIncrease{0};
    int16_t fasterCastRecovery{0};
    int16_t fasterCasting{0};
    int16_t lowerManaCost{0};

    std::vector<Skill> skills;
    std::map<uint16_t, BuffIcon> buffs;
    // Weapon special abilities; bit 0x80 marks "active". Resolved from tiledata by the client.
    std::array<uint16_t, 2> abilities{0xFF, 0xFF};
};

} // namespace uo::world
