// SPDX-License-Identifier: MIT
#include "TextureCache.h"

using namespace ax;

Texture2D* UOTextures::fromImage(const uo::assets::Image& img)
{
    if (img.empty())
        return nullptr;
    auto* tex = new Texture2D();
    if (!tex->initWithData(img.pixels.data(), static_cast<ssize_t>(img.pixels.size() * 4), rhi::PixelFormat::RGBA8,
                           img.width, img.height))
    {
        tex->release();
        return nullptr;
    }
    // UO art is pixel art: keep it crisp when the camera zooms.
    tex->setAliasTexParameters();
    return tex;  // +1 reference, owned by the caller
}

Texture2D* UOTextures::get(Kind kind, std::uint32_t id, std::uint16_t hue)
{
    std::uint64_t key = (static_cast<std::uint64_t>(kind) << 56) | (static_cast<std::uint64_t>(hue) << 32) | id;
    if (auto it = _textures.find(key); it != _textures.end())
        return it->second;

    uo::assets::Image img;
    switch (kind)
    {
    case Kind::Land: img = _install.art().land(id); break;
    case Kind::Static: img = _install.art().statik(id); break;
    case Kind::Gump: img = _install.gumps().get(id, hue); break;
    }

    Texture2D* tex = fromImage(img);
    _textures.emplace(key, tex);  // cache misses too, so missing art is looked up once
    return tex;
}

void UOTextures::clear()
{
    for (auto& [key, tex] : _textures)
    {
        if (tex)
            tex->release();
    }
    _textures.clear();
}
