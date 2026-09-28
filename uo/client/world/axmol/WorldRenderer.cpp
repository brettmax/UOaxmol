// SPDX-License-Identifier: BSD-2-Clause

#include "WorldRenderer.h"

#include <cstddef>

#include "axmol/renderer/ProgramManager.h"
#include "axmol/renderer/Renderer.h"
#include "axmol/renderer/Texture2D.h"
#include "axmol/renderer/VertexLayoutManager.h"
#include "axmol/rhi/GraphicsCore.h"
#include "axmol/rhi/GraphicsDevice.h"
#include "axmol/rhi/ProgramState.h"

#include "HueShader.h"

using namespace ax;

namespace uo::render
{

WorldRenderer* WorldRenderer::create(const ITileData& tiles, ITextureSource& textures)
{
    auto* node = new WorldRenderer(tiles, textures);

    if (node->initRenderer())
    {
        node->autorelease();
        return node;
    }

    delete node;
    return nullptr;
}

WorldRenderer::~WorldRenderer()
{
    _commands.clear();

    for (auto* s : _states)
    {
        AX_SAFE_RELEASE(s);
    }

    AX_SAFE_RELEASE(_baseState);
    AX_SAFE_RELEASE(_vertexBuffer);
    AX_SAFE_RELEASE(_indexBuffer);
}

bool WorldRenderer::initRenderer()
{
    if (!Node::init() || !HueShader::instance().ready())
    {
        return false;
    }

    _program = axpm->loadProgram("custom/uo_world_vs", "custom/uo_world_fs");

    if (!_program)
    {
        return false;
    }

    VertexLayoutDesc desc = axvlm->allocateVertexLayoutDesc();
    desc.startLayout(4);
    desc.addAttrib(_program->getVertexInputDesc(rhi::VertexSemantic::POSITION), rhi::VertexElementType::FLOAT3,
                   offsetof(WorldVertex, x), false);
    desc.addAttrib(_program->getVertexInputDesc(rhi::VertexSemantic::NORMAL), rhi::VertexElementType::FLOAT3,
                   offsetof(WorldVertex, nx), false);
    desc.addAttrib(_program->getVertexInputDesc(rhi::VertexSemantic::TEXCOORD0), rhi::VertexElementType::FLOAT2,
                   offsetof(WorldVertex, u), false);
    desc.addAttrib(_program->getVertexInputDesc(rhi::VertexSemantic::TEXCOORD1), rhi::VertexElementType::FLOAT3,
                   offsetof(WorldVertex, hue), false);
    desc.endLayout(sizeof(WorldVertex));
    _vertexLayout = axvlm->getVertexLayout(std::move(desc));

    _baseState = new rhi::ProgramState(_program);

    return true;
}

void WorldRenderer::setCircleOfTransparency(float radius, float centerX, float centerY)
{
    _cotRadius = radius;
    _cotX      = centerX;
    _cotY      = centerY;
}

rhi::ProgramState* WorldRenderer::stateFor(size_t batch)
{
    // Each command keeps its own state: commands run after draw() returns, so
    // they cannot share one texture binding.
    while (_states.size() <= batch)
    {
        _states.push_back(_baseState->clone());
    }

    return _states[batch];
}

void WorldRenderer::draw(const SceneRenderState& state, const Mat4& transform, uint32_t /*flags*/)
{
    if (!_items || _items->empty())
    {
        return;
    }

    buildWorldGeometry(*_items, _tiles, _textures, _geometry);

    if (_geometry.batches.empty())
    {
        return;
    }

    const Size& size = getContentSize();

    // World pixels are y-down from the node's top-left; Axmol is y-up.
    for (WorldVertex& v : _geometry.vertices)
    {
        v.y = size.height - v.y;
    }

    const size_t vbytes = _geometry.vertices.size() * sizeof(WorldVertex);
    const size_t ibytes = _geometry.indices.size() * sizeof(uint32_t);

    if (vbytes > _vertexCapacity)
    {
        AX_SAFE_RELEASE(_vertexBuffer);
        _vertexCapacity = vbytes * 3 / 2;
        _vertexBuffer   = axdrv->createBuffer(_vertexCapacity, rhi::BufferType::ARRAY_BUFFER, rhi::BufferUsage::DYNAMIC);
    }

    if (ibytes > _indexCapacity)
    {
        AX_SAFE_RELEASE(_indexBuffer);
        _indexCapacity = ibytes * 3 / 2;
        _indexBuffer   = axdrv->createBuffer(_indexCapacity, rhi::BufferType::ELEMENT_ARRAY_BUFFER, rhi::BufferUsage::DYNAMIC);
    }

    _vertexBuffer->updateSubData(_geometry.vertices.data(), 0, vbytes);
    _indexBuffer->updateSubData(_geometry.indices.data(), 0, ibytes);

    const Mat4 mvp = state.getViewProjectionMatrix() * transform;

    const float cotCenter[2] = {_cotX, size.height - _cotY};
    const float radius      = _cotRadius;
    const float bright      = _brightlight;

    rhi::Texture* hueTex   = HueShader::instance().hueTexture()->getRHITexture();
    rhi::Texture* lightTex = HueShader::instance().lightTexture()->getRHITexture();

    if (_commands.size() < _geometry.batches.size())
    {
        _commands.resize(_geometry.batches.size());
    }

    for (size_t i = 0; i < _geometry.batches.size(); ++i)
    {
        const DrawBatch& batch = _geometry.batches[i];
        auto* texture          = static_cast<Texture2D*>(const_cast<void*>(batch.texture));
        rhi::ProgramState* ps  = stateFor(i);

        ps->setUniform(ps->getUniformLocation("u_MVPMatrix"), mvp.m, sizeof(mvp.m));
        ps->setUniform(ps->getUniformLocation("u_cotCenter"), cotCenter, sizeof(cotCenter));
        ps->setUniform(ps->getUniformLocation("u_brightlight"), &bright, sizeof(bright));
        ps->setUniform(ps->getUniformLocation("u_cotRadius"), &radius, sizeof(radius));
        ps->setTexture(ps->getUniformLocation("u_tex0"), 0, texture->getRHITexture());
        ps->setTexture(ps->getUniformLocation("u_hueTex"), 1, hueTex);
        ps->setTexture(ps->getUniformLocation("u_lightTex"), 2, lightTex);

        CustomCommand& cmd = _commands[i];
        cmd.setWeakPSVL(ps, _vertexLayout);
        cmd.setDrawType(CustomCommand::DrawType::ELEMENT);
        cmd.setPrimitiveType(CustomCommand::PrimitiveType::TRIANGLE);
        cmd.setVertexBuffer(_vertexBuffer);
        cmd.setIndexBuffer(_indexBuffer, CustomCommand::IndexFormat::U_INT);
        cmd.setIndexDrawInfo(batch.firstIndex, batch.indexCount);

        // uo_world_fs writes premultiplied colour.
        rhi::BlendDesc& blend           = cmd.blendDesc();
        blend.blendEnabled              = true;
        blend.sourceRGBBlendFactor        = rhi::BlendFactor::ONE;
        blend.destinationRGBBlendFactor   = rhi::BlendFactor::ONE_MINUS_SRC_ALPHA;
        blend.sourceAlphaBlendFactor      = rhi::BlendFactor::ONE;
        blend.destinationAlphaBlendFactor = rhi::BlendFactor::ONE_MINUS_SRC_ALPHA;

        cmd.init(_globalZOrder);
        state.getRenderer()->addCommand(&cmd);
    }
}

}  // namespace uo::render
