// SPDX-License-Identifier: BSD-2-Clause
//
// Axmol node that draws the isometric world: land (flat and stretched), statics
// and their shadows, hued in uo_world_fs.hlsl. The node's content size is the
// viewport in pixels; world pixels are y-down from its top-left, so zoom is
// the node's scale.
//
// Per frame the owner (the game scene) calls setDrawList() with the list built
// by WorldMap::buildDrawList. TextureRegion::texture handles must be
// ax::Texture2D* supplied by the ITextureSource.

#pragma once

#include <vector>

#include "axmol/scene/Node.h"
#include "axmol/renderer/CustomCommand.h"

#include "uo/render/WorldGeometry.h"

namespace uo::render
{

class WorldRenderer : public ax::Node
{
public:
    // HueShader::instance() must be initialised first; it owns the hue texture.
    static WorldRenderer* create(const ITileData& tiles, ITextureSource& textures);

    ~WorldRenderer() override;

    // Items and the WorldObjects they point at must stay alive until the frame
    // is rendered.
    void setDrawList(const std::vector<DrawItem>* items) { _items = items; }

    // ProfileManager.TerrainShadowsLevel * 0.1.
    void setBrightlight(float v) { _brightlight = v; }

    // Radius (0 disables) and centre in world pixels, y-down from the node's
    // top-left. ClassicUO's profile radius is in screen pixels, so pass
    // radius / zoom.
    void setCircleOfTransparency(float radius, float centerX, float centerY);

    void draw(const ax::SceneRenderState& state, const ax::Mat4& transform, uint32_t flags) override;

protected:
    WorldRenderer(const ITileData& tiles, ITextureSource& textures) : _tiles(tiles), _textures(textures) {}
    bool initRenderer();

private:
    ax::rhi::ProgramState* stateFor(size_t batch);

    const ITileData& _tiles;
    ITextureSource& _textures;
    const std::vector<DrawItem>* _items = nullptr;

    WorldGeometry _geometry;

    ax::rhi::Program* _program           = nullptr;
    ax::rhi::VertexLayout* _vertexLayout = nullptr;
    ax::rhi::ProgramState* _baseState    = nullptr;
    std::vector<ax::rhi::ProgramState*> _states;
    std::vector<ax::CustomCommand> _commands;

    ax::rhi::Buffer* _vertexBuffer = nullptr;
    ax::rhi::Buffer* _indexBuffer  = nullptr;
    size_t _vertexCapacity         = 0;
    size_t _indexCapacity          = 0;

    float _brightlight = 0;
    float _cotRadius   = 0;
    float _cotX = 0, _cotY = 0;
};

}  // namespace uo::render
