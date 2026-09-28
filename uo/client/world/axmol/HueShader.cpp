// SPDX-License-Identifier: BSD-2-Clause

#include "HueShader.h"

#include "axmol/2d/Sprite.h"
#include "axmol/renderer/ProgramManager.h"
#include "axmol/renderer/Shaders.h"
#include "axmol/renderer/Texture2D.h"
#include "axmol/rhi/ProgramState.h"

#include "uo/render/HueTexture.h"

using namespace ax;

namespace uo::render
{

HueShader& HueShader::instance()
{
    static HueShader s;
    return s;
}

bool HueShader::init(std::span<const std::uint32_t> packed)
{
    if (packed.size() != static_cast<std::size_t>(kHueTextureWidth) * kHueTextureHeight)
    {
        return false;
    }

    shutdown();

    _hueTexture = new Texture2D();

    if (!_hueTexture->initWithData(packed.data(), static_cast<ssize_t>(packed.size_bytes()), rhi::PixelFormat::RGBA8,
                                   kHueTextureWidth, kHueTextureHeight))
    {
        AX_SAFE_RELEASE_NULL(_hueTexture);
        return false;
    }

    return true;
}

void HueShader::shutdown()
{
    AX_SAFE_RELEASE_NULL(_hueTexture);
    AX_SAFE_RELEASE_NULL(_lightTexture);
}

void HueShader::setLightTexture(Texture2D* lights)
{
    AX_SAFE_RETAIN(lights);
    AX_SAFE_RELEASE(_lightTexture);
    _lightTexture = lights;
}

Texture2D* HueShader::lightTexture() const
{
    return _lightTexture ? _lightTexture : _hueTexture;
}

void HueShader::applyHue(Sprite* sprite, std::uint16_t hue, bool partialHue, bool gump)
{
    applyHueVector(sprite, makeHueVector(hue, partialHue, 1.0f, gump));
}

void HueShader::applyHueVector(Sprite* sprite, const HueVector& hue)
{
    if (!sprite)
    {
        return;
    }

    Texture2D* texture = sprite->getTexture();

    if (hue.mode == static_cast<float>(SHADER_NONE) || !_hueTexture)
    {
        // Back to the stock sprite program for this texture format.
        if (texture)
        {
            sprite->setProgramState(ProgramManager::chooseSpriteProgramType(texture->getPixelFormat()));
        }
        return;
    }

    auto* program = axpm->loadProgram(positionTextureColor_vs, "custom/uo_hue_fs", VertexLayoutKind::Sprite);

    if (!program)
    {
        return;
    }

    rhi::ProgramState* ps = sprite->getProgramState();

    if (!ps || ps->getProgram() != program)
    {
        ps = new rhi::ProgramState(program);
        sprite->setProgramState(ps, true);
    }

    // Sprite opacity still comes through the vertex colour.
    const float vec[4] = {hue.hue, hue.mode, hue.alpha, 0.0f};
    ps->setUniform(ps->getUniformLocation("u_hue"), vec, sizeof(vec));
    ps->setTexture(ps->getUniformLocation("u_hueTex"), 1, _hueTexture->getRHITexture());
}

}  // namespace uo::render
