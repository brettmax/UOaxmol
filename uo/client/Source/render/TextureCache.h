// SPDX-License-Identifier: MIT
#pragma once

#include "axmol/axmol.h"

#include "uo/assets/Image.h"
#include "uo/assets/Installation.h"

#include <cstdint>
#include <unordered_map>

// Turns uocore's decoded RGBA8 images into GPU textures, once per (kind, id, hue). This is the
// Axmol side of ClassicUO's texture atlases; an atlas packer can replace it without changing
// callers.
class UOTextures
{
public:
    enum class Kind : std::uint8_t
    {
        Land,
        Static,
        Gump,
    };

    explicit UOTextures(const uo::assets::Installation& install) : _install(install) {}
    ~UOTextures() { clear(); }

    UOTextures(const UOTextures&)            = delete;
    UOTextures& operator=(const UOTextures&) = delete;

    // nullptr when the id has no art.
    ax::Texture2D* land(std::uint32_t id) { return get(Kind::Land, id, 0); }
    ax::Texture2D* statik(std::uint32_t graphic) { return get(Kind::Static, graphic, 0); }
    ax::Texture2D* gump(std::uint32_t id, std::uint16_t hue = 0) { return get(Kind::Gump, id, hue); }

    static ax::Texture2D* fromImage(const uo::assets::Image& img);

    void clear();

private:
    ax::Texture2D* get(Kind kind, std::uint32_t id, std::uint16_t hue);

    const uo::assets::Installation& _install;
    std::unordered_map<std::uint64_t, ax::Texture2D*> _textures;
};
