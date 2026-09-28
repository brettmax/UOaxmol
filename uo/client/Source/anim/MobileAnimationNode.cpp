// SPDX-License-Identifier: BSD-2-Clause
#include "MobileAnimationNode.h"

namespace uo::client
{

namespace
{
// ClassicUO's DrawShadow: the frame flattened and sheared along the ground, black at half alpha.
constexpr float SHADOW_SCALE_Y = 0.5f;
constexpr float SHADOW_SKEW_X  = -45.f;
constexpr uint8_t SHADOW_ALPHA = 128;
} // namespace

MobileAnimationNode* MobileAnimationNode::create(anim::MobileRenderer& renderer, AnimationTextures& textures)
{
    auto* node = new MobileAnimationNode(renderer, textures);
    if (node->init())
    {
        node->autorelease();
        return node;
    }
    delete node;
    return nullptr;
}

ax::Sprite* MobileAnimationNode::spriteAt(size_t index)
{
    while (_sprites.size() <= index)
    {
        auto* sprite = ax::Sprite::create();
        sprite->setAnchorPoint(ax::Vec2(0.f, 1.f)); // commands give the top-left corner
        addChild(sprite);
        _sprites.push_back(sprite);
    }
    return _sprites[index];
}

const anim::MobileDrawList& MobileAnimationNode::setMobile(const anim::MobileDrawInput& input, int offsetX, int offsetY,
                                                           int offsetZ)
{
    _list = _renderer.build(input, _mirror, 0, 0, offsetX, offsetY, offsetZ);

    size_t used = 0;
    for (const auto& cmd : _list.commands)
    {
        ax::Texture2D* texture = cmd.frame ? _textures.get(*cmd.frame) : nullptr;
        if (!texture)
        {
            continue;
        }

        ax::Sprite* sprite = spriteAt(used);
        sprite->setTexture(texture);
        sprite->setTextureRect(ax::Rect(0, 0, float(cmd.frame->width), float(cmd.frame->height)));
        sprite->setFlippedX(cmd.mirror);
        sprite->setLocalZOrder(static_cast<int>(used)); // the list is already back to front
        sprite->setVisible(true);

        if (cmd.kind == anim::MobileDrawCommand::Kind::Shadow)
        {
            // Shear around the feet: the bottom edge stays put, the top leans away.
            const float h = float(cmd.frame->height);
            sprite->setScaleY(SHADOW_SCALE_Y);
            sprite->setSkewX(SHADOW_SKEW_X);
            sprite->setPosition(float(cmd.x), -float(cmd.y) - h * (1.f - SHADOW_SCALE_Y));
            sprite->setColor(ax::Color32::black);
            sprite->setOpacity(SHADOW_ALPHA);
        }
        else
        {
            sprite->setScaleY(1.f);
            sprite->setSkewX(0.f);
            sprite->setPosition(float(cmd.x), -float(cmd.y));
            sprite->setColor(ax::Color32::white);
            sprite->setOpacity(255);
            if (_hueApplier)
            {
                _hueApplier(sprite, cmd.hue, cmd.partialHue);
            }
        }

        ++used;
    }

    for (size_t i = used; i < _sprites.size(); ++i)
    {
        _sprites[i]->setVisible(false);
    }

    return _list;
}

bool MobileAnimationNode::hitTest(float x, float y) const
{
    return _renderer.hitTest(_list, static_cast<int>(x), static_cast<int>(-y));
}

} // namespace uo::client
