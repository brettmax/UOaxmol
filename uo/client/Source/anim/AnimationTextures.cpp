// SPDX-License-Identifier: BSD-2-Clause
#include "AnimationTextures.h"

namespace uo::client
{

AnimationTextures::~AnimationTextures()
{
    clear();
}

void AnimationTextures::clear()
{
    for (auto& [frame, texture] : _textures)
    {
        AX_SAFE_RELEASE(texture);
    }
    _textures.clear();
}

ax::Texture2D* AnimationTextures::get(const anim::Frame& frame)
{
    if (_generation != _cache.generation())
    {
        clear();
        _generation = _cache.generation();
    }

    if (frame.empty() || frame.pixels.size() != size_t(frame.width) * size_t(frame.height))
    {
        return nullptr;
    }

    if (auto it = _textures.find(&frame); it != _textures.end())
    {
        return it->second;
    }

    // uo::anim pixels are RGBA8 in byte order (see uo/assets/Color.h), straight alpha.
    auto* texture = new ax::Texture2D();
    if (!texture->initWithData(frame.pixels.data(), static_cast<ssize_t>(frame.pixels.size() * sizeof(uint32_t)),
                               ax::rhi::PixelFormat::RGBA8, frame.width, frame.height))
    {
        texture->release();
        return nullptr;
    }
    // Pixel art: no filtering between texels.
    texture->setAliasTexParameters();

    _textures.emplace(&frame, texture);
    return texture;
}

} // namespace uo::client
