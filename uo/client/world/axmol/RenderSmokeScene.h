// SPDX-License-Identifier: BSD-2-Clause
//
// Exercises WorldRenderer and HueShader on a generated map (hills, stretched and flat land,
// hued and partial-hued statics, a hued sprite) without any UO data files, then saves a
// screenshot and quits. Started by the app when UO_RENDER_SMOKE names the output .png.

#pragma once

#include <memory>
#include <string>
#include <vector>

#include "axmol/scene/Scene.h"

#include "uo/render/WorldMap.h"

namespace uo::render
{

class RenderSmokeScene : public ax::Scene
{
public:
    explicit RenderSmokeScene(std::string screenshotPath);
    ~RenderSmokeScene() override;

    bool init() override;
    void update(float dt) override;

    // Hue ramps the smoke scene uses (hue 1 red, hue 2 blue, the rest grey), packed.
    static std::vector<std::uint32_t> smokeHueTexture();

private:
    struct Data;

    std::string _screenshotPath;
    std::unique_ptr<Data> _data;
    std::vector<DrawItem> _drawList;
    int _frames   = 0;
    bool _captured = false;
};

}  // namespace uo::render
