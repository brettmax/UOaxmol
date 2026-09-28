// SPDX-License-Identifier: BSD-2-Clause
// Ported from ClassicUO: Game/GameObjects/Views/MobileView.cs and the animation clock of
// Game/GameObjects/Mobile.cs.
#include "uo/anim/MobileRenderer.h"

#include <algorithm>
#include <array>
#include <cstdlib>

namespace uo::anim
{

namespace
{

constexpr int SIT_OFFSET_Y = 4;

bool isGargoyleDrawBody(uint16_t body, ClientVersion version)
{
    return version >= cv::CV_7000 && (body == 666 || body == 667 || body == 0x02B7 || body == 0x02B6);
}

} // namespace

uint16_t defaultOverrideHue(uint16_t body, bool isDead, bool isHidden)
{
    if (isHidden)
    {
        return 0x038E;
    }
    if (isDead && !isHumanBody(body))
    {
        return 0x0386;
    }
    return 0;
}

void MobileRenderer::drawInternal(MobileDrawList& out, const MobileDrawInput& input, const EquippedItem* entity, int x,
                                  int y, bool mirror, uint8_t frameIndex, bool hasShadow, uint16_t id, uint8_t animGroup,
                                  uint8_t dir, bool isHuman, bool isEquip, bool isMount, int8_t mountOffset,
                                  uint16_t overrideHue, bool charIsSitting, const EquipConvData* equipConv,
                                  MobileDrawCommand::Kind kind, Layer layer)
{
    (void)mountOffset; // ClassicUO slices the sprite at the mount line for depth sorting; a node tree needs no slicing.

    if (id >= _cache.maxAnimationCount())
    {
        return;
    }

    auto result          = _cache.getAnimationFrames(id, animGroup, dir, isEquip, false);
    uint16_t hueFromFile = result.hue;
    if (hueFromFile == 0)
    {
        hueFromFile = overrideHue;
    }

    auto frames = result.frames;
    if (frames.empty())
    {
        return;
    }

    if (frameIndex >= frames.size())
    {
        frameIndex = static_cast<uint8_t>(frames.size() - 1);
    }

    const Frame& sprite = frames[frameIndex % frames.size()];
    const bool hasTexture = !sprite.empty();

    if (!hasTexture)
    {
        // An empty body frame still anchors the sitting layout of the items drawn over it.
        if (!(charIsSitting && entity == nullptr && !hasShadow))
        {
            return;
        }
    }
    else
    {
        x -= mirror ? sprite.width - sprite.centerX : sprite.centerX;
        y -= sprite.height + sprite.centerY;
    }

    MobileDrawCommand cmd;
    cmd.kind       = hasShadow ? MobileDrawCommand::Kind::Shadow : kind;
    cmd.layer      = layer;
    cmd.frame      = hasTexture ? &sprite : nullptr;
    cmd.graphic    = id;
    cmd.action     = animGroup;
    cmd.direction  = dir;
    cmd.frameIndex = frameIndex;
    cmd.x          = x;
    cmd.y          = y;
    cmd.mirror     = mirror;

    if (hasShadow)
    {
        if (hasTexture)
        {
            out.commands.push_back(cmd);
        }
        return;
    }

    uint16_t hue    = overrideHue;
    bool partialHue = false;

    if (hue == 0)
    {
        hue        = entity ? entity->hue : input.hue;
        partialHue = !isMount && entity != nullptr && entity->partialHue;

        if (hue & 0x8000)
        {
            partialHue = true;
            hue &= 0x7FFF;
        }

        if (hue == 0)
        {
            hue = hueFromFile;
            if (hue == 0 && equipConv)
            {
                hue = equipConv->color;
            }
            partialHue = false;
        }
    }

    cmd.hue        = hue;
    cmd.partialHue = partialHue;

    if (hasTexture)
    {
        if (charIsSitting)
        {
            // CalculateSitAnimation
            constexpr float UPPER_BODY_RATIO = 0.35f;
            constexpr float MID_BODY_RATIO   = 0.60f;
            constexpr float LOWER_BODY_RATIO = 0.94f;

            if (entity == nullptr && isHuman)
            {
                const int frameHeight = sprite.height == 0 ? 61 : sprite.height;
                _sit.frameStartY      = y;
                _sit.frameHeight      = frameHeight;
                _sit.waistY           = static_cast<int>(frameHeight * UPPER_BODY_RATIO) + _sit.frameStartY;
                _sit.kneesY           = static_cast<int>(frameHeight * MID_BODY_RATIO) + _sit.frameStartY;
                _sit.feetY            = static_cast<int>(frameHeight * LOWER_BODY_RATIO) + _sit.frameStartY;
            }

            float mx = UPPER_BODY_RATIO;
            float my = MID_BODY_RATIO;
            float mz = LOWER_BODY_RATIO;

            if (entity != nullptr)
            {
                const float h         = static_cast<float>(sprite.height);
                const float itemsEndY = static_cast<float>(y + sprite.height);

                if (y >= _sit.waistY)
                {
                    mx = 0;
                }
                else if (itemsEndY <= _sit.waistY)
                {
                    mx = 1.0f;
                }
                else
                {
                    mx = std::max(0.f, (_sit.waistY - y) / h);
                }

                if (_sit.waistY >= itemsEndY || y >= _sit.kneesY)
                {
                    my = 0;
                }
                else if (_sit.waistY <= y && itemsEndY <= _sit.kneesY)
                {
                    my = 1.0f;
                }
                else
                {
                    float midBodyDiff;
                    if (y >= _sit.waistY)
                    {
                        midBodyDiff = static_cast<float>(_sit.kneesY - y);
                    }
                    else if (itemsEndY <= _sit.kneesY)
                    {
                        midBodyDiff = itemsEndY - _sit.waistY;
                    }
                    else
                    {
                        midBodyDiff = static_cast<float>(_sit.kneesY - _sit.waistY);
                    }
                    my = std::max(0.f, mx + midBodyDiff / h);
                }

                if (itemsEndY <= _sit.kneesY)
                {
                    mz = 0;
                }
                else if (y >= _sit.kneesY)
                {
                    mz = 1.0f;
                }
                else
                {
                    mz = std::max(0.f, my + (itemsEndY - _sit.kneesY) / h);
                }
            }

            cmd.sitting  = true;
            cmd.sitUpper = mx;
            cmd.sitMid   = my;
            cmd.sitLower = mz;
        }

        int xx = -sprite.centerX;
        const int yy = -(sprite.height + sprite.centerY + 3);
        if (mirror)
        {
            xx = -(sprite.width - sprite.centerX);
        }

        out.boundsX      = std::min(out.boundsX, xx);
        out.boundsY      = std::min(out.boundsY, yy);
        out.boundsWidth  = std::max(out.boundsWidth, xx + sprite.width);
        out.boundsHeight = std::max(out.boundsHeight, yy + sprite.height);
    }
    if (entity != nullptr && entity->isLight)
    {
        cmd.isLight = true;
        cmd.lightX  = mirror && hasTexture ? x + sprite.width : x;
        cmd.lightY  = y;
    }

    if (hasTexture)
    {
        out.commands.push_back(cmd);
    }
}

MobileDrawList MobileRenderer::build(const MobileDrawInput& input, bool& isFlipped, int anchorX, int anchorY,
                                     int offsetX, int offsetY, int offsetZ)
{
    MobileDrawList out;
    _sit = {};

    const ClientVersion version = _cache.loader().version();
    const uint16_t overrideHue  = input.overrideHue;

    int posY  = anchorY - 3;
    int drawX = anchorX + offsetX + 22;
    int drawY = posY + (offsetY - offsetZ) + 22;

    const bool hasShadow  = !input.isDead && !input.isHidden && input.shadows;
    const bool isHuman    = isHumanBody(input.body);
    const bool isGargoyle = isGargoyleDrawBody(input.body, version);

    uint8_t dir            = static_cast<uint8_t>(input.direction & 7);
    const uint8_t layerDir = dir;
    AnimationCache::getAnimDirection(dir, isFlipped);

    uint16_t graphic  = graphicForAnimation(input.body);
    uint8_t animGroup = _actions.groupForAnimation(input.anim, graphic);
    uint8_t animIndex = input.animIndex;

    const EquippedItem* mount = input.equipment.find(Layer::Mount);
    int8_t mountOffsetY       = 0;
    bool charSitting          = false;
    uint16_t seatGraphic      = 0;

    if (isHuman && mount && mount->graphic != BOAT_MOUNT_GRAPHIC)
    {
        const uint16_t mountGraphic = mountGraphicForAnimation(mount->graphic, mount->animId);

        if (mountGraphic != 0xFFFF && mountGraphic < _cache.maxAnimationCount())
        {
            if (auto info = findMount(mount->graphic))
            {
                mountOffsetY = info->offsetY;
            }

            const uint8_t animGroupMount = _actions.groupForAnimation(input.anim, mountGraphic);

            if (hasShadow)
            {
                drawInternal(out, input, nullptr, drawX, drawY + 10, isFlipped, animIndex, true, graphic, animGroup, dir,
                             isHuman, false, false, mountOffsetY, overrideHue, false, nullptr,
                             MobileDrawCommand::Kind::Shadow, Layer::Invalid);
                drawInternal(out, input, mount, drawX, drawY, isFlipped, animIndex, true, mountGraphic, animGroupMount,
                             dir, isHuman, false, false, mountOffsetY, overrideHue, false, nullptr,
                             MobileDrawCommand::Kind::Shadow, Layer::Mount);
            }

            drawInternal(out, input, mount, drawX, drawY, isFlipped, animIndex, false, mountGraphic, animGroupMount, dir,
                         isHuman, false, true, mountOffsetY, overrideHue, false, nullptr, MobileDrawCommand::Kind::Mount,
                         Layer::Mount);

            drawY += mountOffsetY;
        }
    }
    else if (input.seat && isHuman && !input.anim.isMounted && !input.anim.isFlying)
    {
        const SittingInfoData& seat = *input.seat;
        seatGraphic                 = seat.graphic;

        animGroup = static_cast<uint8_t>(PeopleAnimationGroup::Stand);
        animIndex = 0;

        dir = static_cast<uint8_t>(input.direction & 7);
        _cache.loader().fixSittingDirection(dir, isFlipped, drawX, drawY, seat);

        drawY += SIT_OFFSET_Y;

        if (dir == 3)
        {
            if (input.anim.isGargoyle)
            {
                drawY -= 30 - SIT_OFFSET_Y;
                animGroup = 42;
            }
            else
            {
                animGroup = 25;
            }
        }
        else if (input.anim.isGargoyle)
        {
            animGroup = 42;
        }
        else
        {
            charSitting = true;
        }
    }
    else if (hasShadow)
    {
        drawInternal(out, input, nullptr, drawX, drawY, isFlipped, animIndex, true, graphic, animGroup, dir, isHuman,
                     false, false, mountOffsetY, overrideHue, charSitting, nullptr, MobileDrawCommand::Kind::Shadow,
                     Layer::Invalid);
    }

    drawInternal(out, input, nullptr, drawX, drawY, isFlipped, animIndex, false, graphic, animGroup, dir, isHuman, false,
                 false, mountOffsetY, overrideHue, charSitting, nullptr, MobileDrawCommand::Kind::Body, Layer::Invalid);

    if (!input.equipment.empty())
    {
        std::array<Layer, PaperdollOrder::N> layers{};
        const int layerCount =
            PaperdollOrder::buildInWorld(input.equipment, input.isFemale || isGargoyle, layerDir, layers);

        for (int i = 0; i < layerCount; ++i)
        {
            const Layer layer         = layers[i];
            const EquippedItem* item  = input.equipment.find(layer);

            if (!item)
            {
                continue;
            }

            if (input.isDead && (layer == Layer::Hair || layer == Layer::Beard))
            {
                continue;
            }

            if (!isHuman)
            {
                if (item->isLight)
                {
                    out.undrawnLights.push_back(layer);
                }
                continue;
            }

            if (isCovered(input.equipment, input.body, layer))
            {
                continue;
            }

            if (item->animId == 0)
            {
                if (item->isLight)
                {
                    out.undrawnLights.push_back(layer);
                }
                continue;
            }

            uint16_t itemGraphic = item->animId;
            if (isGargoyle)
            {
                itemGraphic = fixGargoyleEquipment(itemGraphic);
            }

            const EquipConvData* conv = _cache.loader().findEquipConv(input.body, item->animId);
            if (conv)
            {
                itemGraphic = conv->graphic;
            }

            const uint8_t group = isGargoyle && seatGraphic == 0 ? _actions.groupForAnimation(input.anim, itemGraphic)
                                                                 : animGroup;

            drawInternal(out, input, item, drawX, drawY, isFlipped, animIndex, false, itemGraphic, group, dir, isHuman,
                         true, false, mountOffsetY, overrideHue, charSitting, conv,
                         MobileDrawCommand::Kind::Equipment, layer);
        }
    }

    out.boundsX      = std::abs(out.boundsX);
    out.boundsY      = std::abs(out.boundsY);
    out.boundsWidth  = out.boundsX + out.boundsWidth;
    out.boundsHeight = out.boundsY + out.boundsHeight;

    return out;
}

bool MobileRenderer::hitTest(const MobileDrawList& list, int x, int y) const
{
    for (const auto& cmd : list.commands)
    {
        if (cmd.kind == MobileDrawCommand::Kind::Shadow || !cmd.frame)
        {
            continue;
        }

        const int w = cmd.frame->width;
        const int h = cmd.frame->height;
        int lx      = x - cmd.x;
        const int ly = y - cmd.y;

        if (lx < 0 || ly < 0 || lx >= w || ly >= h)
        {
            continue;
        }

        if (cmd.mirror)
        {
            lx = w - 1 - lx;
        }

        if (cmd.frame->hitTest(lx, ly))
        {
            return true;
        }
    }
    return false;
}

void MobileAnimClock::setAnimation(MobileAnimState& state, uint8_t id, uint32_t now, uint8_t intervalValue,
                                   uint8_t frameCount, uint16_t repeatCount, bool repeatValue, bool forwardValue,
                                   bool fromServer)
{
    state.animationGroup      = id;
    animIndex                 = forwardValue ? 0 : frameCount;
    interval                  = intervalValue;
    animationFrameCount       = forwardValue ? 0 : frameCount;
    repeatMode                = repeatCount;
    repeatModeCount           = repeatCount;
    repeat                    = repeatValue;
    forward                   = forwardValue;
    state.animationFromServer = fromServer;
    lastChange                = now;
}

MobileAnimClock::Result MobileAnimClock::tick(MobileAnimState& state, uint32_t now, int frames, bool noIterate)
{
    if (lastChange >= now || noIterate)
    {
        return Result::Waiting;
    }

    uint32_t currentDelay = CHARACTER_ANIMATION_DELAY;
    Result result         = Result::Advanced;

    if (frames <= 0)
    {
        lastChange = now + currentDelay;
        return Result::NoFrames;
    }

    int fc         = frames;
    int frameIndex = animIndex + (state.animationFromServer && !forward ? -1 : 1);

    if (state.animationFromServer)
    {
        currentDelay += currentDelay * (interval + 1u);

        if (animationFrameCount == 0)
        {
            animationFrameCount = static_cast<uint8_t>(fc);
        }
        else
        {
            fc = animationFrameCount;
        }

        bool ended = false;
        if (forward && frameIndex >= fc)
        {
            frameIndex = 0;
            ended      = true;
        }
        else if (!forward && frameIndex < 0)
        {
            frameIndex = fc == 0 ? 0 : frames - 1;
            ended      = true;
        }

        if (ended && repeatMode != 0 && --repeatMode == 0)
        {
            if (repeat)
            {
                repeatModeCount = repeatMode;
                repeat          = false;
            }
            else
            {
                setAnimation(state, 0xFF, now);
            }
        }
    }
    else if (frameIndex >= fc)
    {
        frameIndex = 0;
        result     = Result::LoopedToStart;
    }

    animIndex  = static_cast<uint8_t>(frameIndex % frames);
    lastChange = now + currentDelay;
    return result;
}

} // namespace uo::anim
