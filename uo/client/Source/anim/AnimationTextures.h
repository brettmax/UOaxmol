// SPDX-License-Identifier: BSD-2-Clause
// Part of the AxmolUO client. Replaces the TextureAtlas half of ClassicUO.Renderer
// Animations: uploads decoded animation frames as Axmol textures on first use.
#pragma once

#include "uo/anim/AnimationCache.h"

#include "axmol/renderer/Texture2D.h"

#include <cstdint>
#include <unordered_map>

namespace uo::client
{

// Textures for uo::anim frames, keyed by frame address (stable for the cache's lifetime).
// Flushes itself when the cache's generation changes (Bodyconv applied, cache cleared).
class AnimationTextures
{
public:
    explicit AnimationTextures(anim::AnimationCache& cache) : _cache(cache) {}
    ~AnimationTextures();

    AnimationTextures(const AnimationTextures&)            = delete;
    AnimationTextures& operator=(const AnimationTextures&) = delete;

    // The frame's texture, created on first use; nullptr for an empty frame.
    ax::Texture2D* get(const anim::Frame& frame);

    void clear();
    size_t size() const { return _textures.size(); }

private:
    anim::AnimationCache& _cache;
    uint32_t _generation = 0;
    std::unordered_map<const anim::Frame*, ax::Texture2D*> _textures;
};

} // namespace uo::client
