// SPDX-License-Identifier: BSD-2-Clause
// Ported from ClassicUO (ClassicUO.Client/Game/GameObjects/MobileAnimation.cs). The label
// names (LABEL_190, LABEL_222, v13) come from the decompiled client ClassicUO follows and are
// kept so the two can be compared.
#include "uo/anim/MobileAnimation.h"

#include <random>

namespace uo::anim
{

namespace
{

constexpr uint16_t HANDS_BASE_ANIMID[] = {0x0263, 0x0264, 0x0265, 0x0266, 0x0267, 0x0268, 0x0269,
                                          0x026D, 0x0270, 0x0272, 0x0274, 0x027A, 0x027C, 0x027F,
                                          0x0281, 0x0286, 0x0288, 0x0289, 0x028B, 0};

constexpr uint16_t HAND2_BASE_ANIMID[] = {0x0240, 0x0241, 0x0242, 0x0243, 0x0244, 0x0245, 0x0246, 0x03E0, 0x03E1, 0};

bool has(uint32_t flags, uint32_t f)
{
    return (flags & f) != 0;
}

} // namespace

uint16_t graphicForAnimation(uint16_t g)
{
    switch (g)
    {
    case 0x0192:
    case 0x0193: return static_cast<uint16_t>(g - 2);
    case 0x02B6: return 667;
    case 0x02B7: return 666;
    default: return g;
    }
}

bool isHumanBody(uint16_t g)
{
    return (g >= 0x0190 && g <= 0x0193) || (g >= 0x00B7 && g <= 0x00BA) || (g >= 0x025D && g <= 0x0260) ||
           g == 0x029A || g == 0x029B || g == 0x02B6 || g == 0x02B7 || g == 0x03DB || g == 0x03DF || g == 0x03E2 ||
           g == 0x02E8 || g == 0x02E9 || g == 0x04E5;
}

bool isGargoyleBody(uint16_t g, ClientVersion version)
{
    return (version >= cv::CV_7000 && g == 0x029A) || g == 0x029B;
}

MobileAnimation::MobileAnimation(AnimationCache& cache) : _cache(cache)
{
    _random = [rng = std::make_shared<std::mt19937>(std::random_device{}())]() { return static_cast<uint32_t>((*rng)()); };
}

void MobileAnimation::calculateHeight(uint16_t graphic, const MobileAnimState& mobile, uint32_t flags, bool isrun,
                                      bool iswalking, uint8_t& result)
{
    if (has(flags, AnimationFlags::CalculateOffsetByPeopleGroup))
    {
        if (result == 0xFF)
        {
            result = 0;
        }
    }
    else if (has(flags, AnimationFlags::CalculateOffsetByLowGroup))
    {
        if (!iswalking)
        {
            if (result == 0xFF)
            {
                result = 2;
            }
        }
        else if (isrun)
        {
            result = 1;
        }
        else
        {
            result = 0;
        }
    }
    else
    {
        if (mobile.isFlying)
        {
            result = 19;
        }
        else if (!iswalking)
        {
            if (result == 0xFF)
            {
                if (has(flags, AnimationFlags::IdleAt8Frame) && _cache.animationExists(graphic, 8))
                {
                    result = 8;
                }
                else if (has(flags, AnimationFlags::UseUopAnimation) && !mobile.inWarMode)
                {
                    result = 25;
                }
                else
                {
                    result = 1;
                }
            }
        }
        else if (isrun)
        {
            if (has(flags, AnimationFlags::CanFlying) && _cache.animationExists(graphic, 19))
            {
                result = 19;
            }
            else
            {
                result = has(flags, AnimationFlags::UseUopAnimation) ? 24 : 0;
            }
        }
        else
        {
            result = (has(flags, AnimationFlags::UseUopAnimation) && !mobile.inWarMode) ? 22 : 0;
        }
    }
}

void MobileAnimation::label222(uint32_t flags, uint16_t& v13)
{
    if (has(flags, AnimationFlags::CalculateOffsetLowGroupExtended))
    {
        switch (v13)
        {
        case 0: v13 = 0; break;
        case 1: v13 = 19; break;
        case 5:
        case 6:
            if (has(flags, AnimationFlags::IdleAt8Frame))
            {
                v13 = 4;
            }
            else
            {
                v13 = static_cast<uint16_t>(6 - (_random() % 2 != 0 ? 1 : 0));
            }
            break;
        case 8: v13 = 2; break;
        case 9: v13 = 17; break;
        case 10:
            v13 = 18;
            if (has(flags, AnimationFlags::IdleAt8Frame))
            {
                --v13;
            }
            break;
        case 12: v13 = 3; break;
        default: v13 = 1; break; // LABEL_241
        }
    }
    else if (has(flags, AnimationFlags::CalculateOffsetByLowGroup))
    {
        switch (v13)
        {
        case 0: v13 = 0; break;
        case 2: v13 = 8; break;
        case 3: v13 = 12; break;
        case 4:
        case 6:
        case 7:
        case 8:
        case 9:
        case 12:
        case 13:
        case 14: v13 = 5; break;
        case 5: v13 = 6; break;
        case 10:
        case 21: v13 = 7; break;
        case 11: v13 = 3; break;
        case 17: v13 = 9; break;
        case 18: v13 = 10; break;
        case 19: v13 = 1; break;
        default: v13 = 2; break;
        }
    }

    // LABEL_243
    v13 = static_cast<uint16_t>(v13 & 0x7F);
}

void MobileAnimation::label190(uint32_t flags, uint16_t& v13)
{
    if (has(flags, AnimationFlags::Unknown80) && v13 == 4)
    {
        v13 = 5;
    }

    if (has(flags, AnimationFlags::Unknown200))
    {
        // `v13 - 7 > 9` in int arithmetic: true only for v13 > 16.
        if (int(v13) - 7 > 9)
        {
            if (v13 == 19)
            {
                v13 = 0;
            }
            else if (v13 > 19)
            {
                v13 = 1;
            }

            label222(flags, v13);
            return;
        }
    }
    else
    {
        if (has(flags, AnimationFlags::Unknown100))
        {
            switch (v13)
            {
            case 10:
            case 15:
            case 16: v13 = 1; break;
            case 11: v13 = 17; break;
            default: break;
            }

            label222(flags, v13);
            return;
        }

        if (has(flags, AnimationFlags::Unknown1))
        {
            if (v13 == 21)
            {
                v13 = 10;
            }

            label222(flags, v13);
            return;
        }

        if (!has(flags, AnimationFlags::CalculateOffsetByPeopleGroup))
        {
            label222(flags, v13);
            return;
        }

        bool mapped = true;
        switch (v13)
        {
        case 0: mapped = false; break; // ClassicUO's `case 0: v13 = 0; break;` falls to v13 = 4 below
        case 2: v13 = 21; break;
        case 3: v13 = 22; break;
        case 4:
        case 9: v13 = 9; break;
        case 5: v13 = 11; break;
        case 6: v13 = 13; break;
        case 7: v13 = 18; break;
        case 8: v13 = 19; break;
        case 10:
        case 21: v13 = 20; break;
        case 11: v13 = 3; break;
        case 12:
        case 14: v13 = 16; break;
        case 13: v13 = 17; break;
        case 15:
        case 16: v13 = 30; break;
        case 17: v13 = 5; break;
        case 18: v13 = 6; break;
        case 19: v13 = 1; break;
        default: mapped = false; break;
        }

        if (mapped)
        {
            label222(flags, v13);
            return;
        }
    }

    v13 = 4;
    label222(flags, v13);
}

uint8_t MobileAnimation::groupForAnimation(const MobileAnimState& mobile, uint16_t checkGraphic)
{
    uint16_t graphic = checkGraphic;
    if (graphic == 0)
    {
        graphic = graphicForAnimation(mobile.graphic);
    }

    if (graphic >= _cache.maxAnimationCount())
    {
        return 0;
    }

    AnimationGroupsType originalType = _cache.getAnimType(graphic);
    _cache.convertBodyIfNeeded(graphic);
    const AnimationGroupsType type = _cache.getAnimType(graphic);
    const uint32_t flags           = _cache.getAnimFlags(graphic);
    const bool uop                 = has(flags, AnimationFlags::UseUopAnimation);

    if (mobile.animationFromServer && mobile.animationGroup != 0xFF)
    {
        uint16_t v13 = mobile.animationGroup;

        if (v13 == 12)
        {
            if (!(type == AnimationGroupsType::Human || type == AnimationGroupsType::Equipment ||
                  has(flags, AnimationFlags::Unknown1000)))
            {
                if (type != AnimationGroupsType::Monster)
                {
                    // The Human/Equipment test repeats the one excluded above, so this is always 5.
                    v13 = (type == AnimationGroupsType::Human || type == AnimationGroupsType::Equipment) ? 16 : 5;
                }
                else
                {
                    v13 = 4;
                }
            }
        }

        if (type != AnimationGroupsType::Monster)
        {
            if (type != AnimationGroupsType::SeaMonster)
            {
                if (type == AnimationGroupsType::Animal)
                {
                    if (isReplacedObjectAnimation(0, v13))
                    {
                        originalType = AnimationGroupsType::Unknown;
                    }

                    if (v13 > 12)
                    {
                        switch (v13)
                        {
                        case 23: v13 = 0; break;
                        case 24: v13 = 1; break;
                        case 26:
                            if (!_cache.animationExists(graphic, 26) ||
                                (mobile.inWarMode && _cache.animationExists(graphic, 9)))
                            {
                                v13 = 9;
                            }
                            break;
                        case 28: v13 = _cache.animationExists(graphic, 10) ? 10 : 5; break;
                        default: v13 = 2; break;
                        }
                    }
                }
                else if (isReplacedObjectAnimation(1, v13))
                {
                    label190(flags, v13);
                    return static_cast<uint8_t>(v13);
                }
            }
            else
            {
                if (isReplacedObjectAnimation(3, v13))
                {
                    originalType = AnimationGroupsType::Unknown;
                }

                if (v13 > 8)
                {
                    v13 = 2;
                }
            }
        }
        else
        {
            if (isReplacedObjectAnimation(2, v13))
            {
                originalType = AnimationGroupsType::Unknown;
            }

            if (!_cache.animationExists(graphic, static_cast<uint8_t>(v13)))
            {
                v13 = 1;
            }

            if (!uop && v13 > 21)
            {
                v13 = 1;
            }
        }

        if (originalType == AnimationGroupsType::Unknown)
        {
            label190(flags, v13);
            return static_cast<uint8_t>(v13);
        }

        if (originalType != AnimationGroupsType::Monster)
        {
            if (originalType == AnimationGroupsType::Animal && type == AnimationGroupsType::Monster)
            {
                switch (v13)
                {
                case 0: v13 = 0; break;
                case 1: v13 = 19; break;
                case 3: v13 = 11; break;
                case 5: v13 = 4; break;
                case 6: v13 = 5; break;
                case 7:
                case 11: v13 = 10; break;
                case 8: v13 = 2; break;
                case 9: v13 = 17; break;
                case 10: v13 = 18; break;
                case 12: v13 = 3; break;
                default: v13 = 1; break; // LABEL_187
                }
            }

            label190(flags, v13);
            return static_cast<uint8_t>(v13);
        }

        switch (type)
        {
        case AnimationGroupsType::Human:
            switch (v13)
            {
            case 0: v13 = 0; break;
            case 2: v13 = 21; break;
            case 3: v13 = 22; break;
            case 4:
            case 9: v13 = 9; break;
            case 5: v13 = 11; break;
            case 6: v13 = 13; break;
            case 7: v13 = 18; break;
            case 8: v13 = 19; break;
            case 10:
            case 21: v13 = 20; break;
            case 12:
            case 14: v13 = 16; break;
            case 13: v13 = 17; break;
            case 15:
            case 16: v13 = 30; break;
            case 17: v13 = 5; break;
            case 18: v13 = 6; break;
            case 19: v13 = 1; break;
            default: v13 = 4; break; // LABEL_161
            }
            break;

        case AnimationGroupsType::Animal:
            switch (v13)
            {
            case 0: v13 = 0; break;
            case 2: v13 = 8; break;
            case 3: v13 = 12; break;
            case 4:
            case 6:
            case 7:
            case 8:
            case 9:
            case 12:
            case 13:
            case 14: v13 = 5; break;
            case 5: v13 = 6; break;
            case 10:
            case 21: v13 = 7; break;
            case 11: v13 = 3; break;
            case 17: v13 = 9; break;
            case 18: v13 = 10; break;
            case 19: v13 = 1; break;
            default: v13 = 2; break;
            }
            break;

        case AnimationGroupsType::SeaMonster:
            switch (v13)
            {
            case 0: v13 = 0; break;
            case 2:
            case 3: v13 = 8; break;
            case 4:
            case 6:
            case 7:
            case 8:
            case 9:
            case 12:
            case 13:
            case 14: v13 = 5; break;
            case 5: v13 = 6; break;
            case 10:
            case 21: v13 = 7; break;
            case 17: v13 = 3; break;
            case 18: v13 = 4; break;
            case 19: break;
            default: v13 = 2; break;
            }
            break;

        default: break; // LABEL_189
        }

        label190(flags, v13);
        return static_cast<uint8_t>(v13);
    }

    uint8_t result = mobile.animationGroup;
    const bool isWalking = mobile.isWalking;
    const bool isRun     = mobile.isRunning;

    switch (type)
    {
    case AnimationGroupsType::Animal:
        if (has(flags, AnimationFlags::CalculateOffsetLowGroupExtended))
        {
            calculateHeight(graphic, mobile, flags, isRun, isWalking, result);
        }
        else if (!isWalking)
        {
            if (result == 0xFF)
            {
                if (uop)
                {
                    result = (mobile.inWarMode && _cache.animationExists(graphic, 1)) ? 1 : 25;
                }
                else
                {
                    result = 2;
                }
            }
        }
        else if (isRun)
        {
            if (uop)
            {
                result = 24;
            }
            else
            {
                result = _cache.animationExists(graphic, 1) ? 1 : 2;
            }
        }
        else if (uop && (!mobile.inWarMode || !_cache.animationExists(graphic, 0)))
        {
            result = 22;
        }
        else
        {
            result = 0;
        }
        break;

    case AnimationGroupsType::Monster: calculateHeight(graphic, mobile, flags, isRun, isWalking, result); break;

    case AnimationGroupsType::SeaMonster:
        if (!isWalking)
        {
            if (result == 0xFF)
            {
                result = 2;
            }
        }
        else
        {
            result = isRun ? 1 : 0;
        }
        break;

    default:
    {
        const MobileAnimState::Hand* hand2 = mobile.twoHanded.present ? &mobile.twoHanded : nullptr;
        const bool gargoyleFlying          = mobile.isGargoyle && mobile.isFlying;

        if (!isWalking)
        {
            if (result == 0xFF)
            {
                const bool haveLightAtHand2 = hand2 && hand2->isLight && hand2->animId == graphic;

                if (mobile.isMounted)
                {
                    result = haveLightAtHand2 ? 28 : 25;
                }
                else if (gargoyleFlying) // TODO (ClassicUO): what's up when it is dead?
                {
                    result = mobile.inWarMode ? 65 : 64;
                }
                else if (!mobile.inWarMode || mobile.isDead)
                {
                    if (haveLightAtHand2)
                    {
                        result = 0;
                    }
                    else if (uop && type == AnimationGroupsType::Equipment && _cache.animationExists(graphic, 37))
                    {
                        result = 37;
                    }
                    else
                    {
                        result = 4;
                    }
                }
                else if (haveLightAtHand2)
                {
                    result = 2;
                }
                else
                {
                    const MobileAnimState::Hand* hand1 = mobile.oneHanded.present ? &mobile.oneHanded : nullptr;
                    const uint16_t handAnimIds[2]       = {hand1 ? hand1->animId : uint16_t(0),
                                                           hand2 ? hand2->animId : uint16_t(0)};

                    if (!hand1)
                    {
                        if (hand2)
                        {
                            if (uop && type == AnimationGroupsType::Equipment && !_cache.animationExists(graphic, 7))
                            {
                                result = 8;
                            }
                            else
                            {
                                result = 7;
                            }

                            bool found = false;
                            for (int i = 0; i < 2 && !found; ++i)
                            {
                                if (handAnimIds[i] >= 0x0263 && handAnimIds[i] <= 0x028B)
                                {
                                    for (uint16_t base : HANDS_BASE_ANIMID)
                                    {
                                        if (handAnimIds[i] == base)
                                        {
                                            result = 8;
                                            found  = true;
                                            break;
                                        }
                                    }
                                }
                            }
                        }
                        else if (gargoyleFlying)
                        {
                            result = 64;
                        }
                        else
                        {
                            result = 7;
                        }
                    }
                    else
                    {
                        result = 7;
                    }
                }
            }
        }
        else if (mobile.isMounted)
        {
            result = isRun ? 24 : 23;
        }
        else if (isRun || !mobile.inWarMode || mobile.isDead)
        {
            if (has(flags, AnimationFlags::UseUopAnimation))
            {
                if (gargoyleFlying)
                {
                    result = isRun ? 63 : 62;
                }
                else if (isRun && _cache.animationExists(graphic, 24))
                {
                    result = 24;
                }
                else if (isRun)
                {
                    if (uop && type == AnimationGroupsType::Equipment && !_cache.animationExists(graphic, 2))
                    {
                        result = 3;
                    }
                    else
                    {
                        result = 2;
                        if (mobile.isGargoyle)
                        {
                            hand2 = mobile.oneHanded.present ? &mobile.oneHanded : nullptr;
                        }
                    }
                }
                else
                {
                    if (uop && type == AnimationGroupsType::Equipment && !_cache.animationExists(graphic, 0))
                    {
                        result = 1;
                    }
                    else
                    {
                        result = 0;
                    }
                }
            }
            else if (isRun)
            {
                result = hand2 ? 3 : 2;
            }
            else
            {
                result = hand2 ? 1 : 0;
            }

            if (hand2)
            {
                const uint16_t hand2Graphic = hand2->animId;

                if (hand2Graphic < 0x0240 || hand2Graphic > 0x03E1)
                {
                    if (gargoyleFlying)
                    {
                        result = isRun ? 63 : 62;
                    }
                    else
                    {
                        result = isRun ? 3 : 1;
                    }
                }
                else
                {
                    for (uint16_t base : HAND2_BASE_ANIMID)
                    {
                        if (base == hand2Graphic)
                        {
                            if (gargoyleFlying)
                            {
                                result = isRun ? 63 : 62;
                            }
                            else
                            {
                                result = isRun ? 3 : 1;
                            }
                            break;
                        }
                    }
                }
            }
        }
        else if (gargoyleFlying)
        {
            result = 62;
        }
        else
        {
            result = 15;
        }
        break;
    }
    }

    return result;
}

bool MobileAnimation::isReplacedObjectAnimation(uint8_t anim, uint16_t v13) const
{
    const auto& replaces = _cache.loader().groupReplaces();
    if (anim < replaces.size())
    {
        for (const auto& [group, replace] : replaces[anim])
        {
            if (group == v13)
            {
                return replace != 0xFF;
            }
        }
    }
    return false;
}

uint8_t MobileAnimation::replacedObjectAnimation(uint16_t graphic, uint16_t index)
{
    auto replacedGroup = [](const std::vector<std::pair<uint16_t, uint8_t>>& list, uint16_t idx, uint16_t walkIdx) -> uint16_t {
        for (const auto& [group, replace] : list)
        {
            if (group == idx)
            {
                return replace == 0xFF ? walkIdx : replace;
            }
        }
        return idx;
    };

    const auto& replaces = _cache.loader().groupReplaces();
    const AnimationGroups group = _cache.loader().getGroupIndex(graphic, _cache.getAnimType(graphic));

    if (group == AnimationGroups::Low)
    {
        return static_cast<uint8_t>(replacedGroup(replaces[0], index, uint16_t(LowAnimationGroup::Walk)) %
                                    uint16_t(LowAnimationGroup::AnimationCount));
    }

    if (group == AnimationGroups::People)
    {
        return static_cast<uint8_t>(replacedGroup(replaces[1], index, uint16_t(PeopleAnimationGroup::WalkUnarmed)) %
                                    uint16_t(PeopleAnimationGroup::AnimationCount));
    }

    return static_cast<uint8_t>(index % uint16_t(HighAnimationGroup::AnimationCount));
}

uint8_t MobileAnimation::objectNewAnimation(const MobileAnimState& mobile, uint16_t type, uint16_t action, uint8_t mode)
{
    if (mobile.graphic >= _cache.maxAnimationCount())
    {
        return 0;
    }

    const uint16_t graphic = mobile.graphic;
    const uint32_t flags   = _cache.getAnimFlags(graphic);
    // Without a mobtypes.txt entry (no Found bit) the body is treated as a monster.
    const AnimationGroupsType animType =
        has(flags, AnimationFlags::Found) ? _cache.getAnimType(graphic) : AnimationGroupsType::Monster;
    const bool gargoyleFlying = mobile.isGargoyle && mobile.isFlying;

    switch (type)
    {
    case 0: // attack
    {
        if (action > 10)
        {
            return 0;
        }

        if (animType == AnimationGroupsType::Monster)
        {
            switch (mode % 4)
            {
            case 1: return 5;
            case 2: return 6;
            case 3:
                if (has(flags, AnimationFlags::Unknown1))
                {
                    return 12;
                }
                return 4;
            default: return 4;
            }
        }

        if (animType == AnimationGroupsType::SeaMonster)
        {
            return mode % 2 != 0 ? 6 : 5;
        }

        if (animType != AnimationGroupsType::Animal)
        {
            if (mobile.isMounted)
            {
                if (action > 0)
                {
                    if (action == 1)
                    {
                        return 27;
                    }
                    if (action == 2)
                    {
                        return 28;
                    }
                    return 26;
                }
                return 29;
            }

            switch (action)
            {
            case 1: return 18;
            case 2: return 19;
            case 3: return 11;
            case 4: return 9;
            case 5: return 10;
            case 6: return 12;
            case 7:
                if (gargoyleFlying && _cache.animationExists(graphic, 72))
                {
                    return 72;
                }
                return 13;
            case 8: return 14;
            default:
                if (gargoyleFlying && _cache.animationExists(graphic, 71))
                {
                    return 71;
                }
                if (_cache.animationExists(graphic, 31))
                {
                    return 31;
                }
                break;
            }
        }

        if (has(flags, AnimationFlags::Use2IfHittedWhileRunning))
        {
            return 2;
        }

        if (mode % 2 != 0 && _cache.animationExists(graphic, 6))
        {
            return 6;
        }

        return 5;
    }

    case 1: // parry
    case 2:
        if (animType != AnimationGroupsType::Monster)
        {
            if (animType <= AnimationGroupsType::Animal || mobile.isMounted)
            {
                return 0xFF;
            }
            return 30;
        }
        return mode % 2 != 0 ? 15 : 16;

    case 3: // die
        if (animType != AnimationGroupsType::Monster)
        {
            if (animType == AnimationGroupsType::SeaMonster)
            {
                return 8;
            }
            if (animType == AnimationGroupsType::Animal)
            {
                return mode % 2 != 0 ? 21 : 22;
            }
            return mode % 2 != 0 ? 8 : 12;
        }
        return mode % 2 != 0 ? 2 : 3;

    case 4: // get hit
        if (animType != AnimationGroupsType::Monster)
        {
            if (animType > AnimationGroupsType::Animal)
            {
                if (gargoyleFlying && _cache.animationExists(graphic, 77))
                {
                    return 77;
                }
                if (mobile.isMounted)
                {
                    return 0xFF;
                }
                return 20;
            }
            return 7;
        }
        return 10;

    case 5: // fidget
        if (animType <= AnimationGroupsType::SeaMonster)
        {
            return mode % 2 != 0 ? 18 : 17;
        }
        if (animType != AnimationGroupsType::Animal)
        {
            if (mobile.isMounted)
            {
                return 0xFF;
            }
            return mode % 2 != 0 ? 6 : 5;
        }
        switch (mode % 3)
        {
        case 1: return 10;
        case 2: return 3;
        default: return 9;
        }

    case 6: // eat
    case 14:
        if (animType != AnimationGroupsType::Monster)
        {
            if (animType != AnimationGroupsType::SeaMonster)
            {
                if (animType == AnimationGroupsType::Animal)
                {
                    return 3;
                }
                if (mobile.isMounted)
                {
                    return 0xFF;
                }
                return 34;
            }
            return 5;
        }
        return 11;

    case 7: // emote
        if (mobile.isMounted)
        {
            return 0xFF;
        }
        if (action > 0)
        {
            if (action == 1)
            {
                return 33;
            }
        }
        else
        {
            return 32;
        }
        return 0;

    case 8: // alert
        if (animType != AnimationGroupsType::Monster)
        {
            if (animType != AnimationGroupsType::SeaMonster)
            {
                if (animType == AnimationGroupsType::Animal)
                {
                    return 9;
                }
                return mobile.isMounted ? 0xFF : 33;
            }
            return 3;
        }
        return 11;

    case 9: // take off
        if (animType != AnimationGroupsType::Monster)
        {
            if (mobile.isGargoyle && action == 0)
            {
                return 60;
            }
            return 0xFF;
        }
        return 20;

    case 10: // land
        if (animType != AnimationGroupsType::Monster)
        {
            if (mobile.isGargoyle && action == 0)
            {
                return 61;
            }
            return 0xFF;
        }
        return 20;

    case 11: // spell
        if (animType != AnimationGroupsType::Monster)
        {
            if (animType >= AnimationGroupsType::Animal)
            {
                if (mobile.isMounted)
                {
                    return 0xFF;
                }
                switch (action)
                {
                case 1:
                case 2:
                    if (gargoyleFlying)
                    {
                        return 76;
                    }
                    return 17;
                default: break;
                }
                if (gargoyleFlying)
                {
                    return 75;
                }
                return 16;
            }
            return 5;
        }
        return 12;

    default: return 0;
    }
}

} // namespace uo::anim
