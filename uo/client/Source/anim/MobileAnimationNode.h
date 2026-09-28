// SPDX-License-Identifier: BSD-2-Clause
// Part of the AxmolUO client. The Axmol half of ClassicUO's MobileView: draws the sprite
// list uo::anim::MobileRenderer builds (shadows, mount, body, equipment) as child sprites.
#pragma once

#include "AnimationTextures.h"

#include "uo/anim/MobileRenderer.h"

#include "axmol/scene/Node.h"
#include "axmol/2d/Sprite.h"

#include <functional>
#include <vector>

namespace uo::client
{

// One animated mobile. Place the node at the screen point of the mobile's tile anchor
// (ClassicUO's posX/posY; y grows up in Axmol, the renderer's offsets are flipped here),
// then call setMobile every frame or whenever the mobile changes.
class MobileAnimationNode : public ax::Node
{
public:
    // Applies a UO hue to a sprite. The hue shader belongs to the renderer; until one is set,
    // parts draw unhued.
    using HueApplier = std::function<void(ax::Sprite*, uint16_t hue, bool partialHue)>;

    static MobileAnimationNode* create(anim::MobileRenderer& renderer, AnimationTextures& textures);

    void setHueApplier(HueApplier applier) { _hueApplier = std::move(applier); }

    // Rebuilds the parts for this state. Returns the draw list (bounds, lights) for picking.
    const anim::MobileDrawList& setMobile(const anim::MobileDrawInput& input, int offsetX = 0, int offsetY = 0,
                                          int offsetZ = 0);

    // Pixel-exact picking; x/y in node space (Axmol, y up).
    bool hitTest(float x, float y) const;

    const anim::MobileDrawList& drawList() const { return _list; }

private:
    MobileAnimationNode(anim::MobileRenderer& renderer, AnimationTextures& textures)
        : _renderer(renderer), _textures(textures)
    {}

    ax::Sprite* spriteAt(size_t index);

    anim::MobileRenderer& _renderer;
    AnimationTextures& _textures;
    HueApplier _hueApplier;
    anim::MobileDrawList _list;
    std::vector<ax::Sprite*> _sprites;
    bool _mirror = false; // ClassicUO's persistent IsFlipped
};

} // namespace uo::client
