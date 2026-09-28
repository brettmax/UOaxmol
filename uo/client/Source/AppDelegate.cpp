/****************************************************************************
 Copyright (c) 2017-2018 Xiamen Yaji Software Co., Ltd.
 Copyright (c) 2019-present Simdsoft Limited.

 https://axmol.dev/

 SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "AppDelegate.h"
#include "GameClient.h"
#include "scenes/LoginScene.h"
#include "world/axmol/RenderSmokeScene.h"

#include <cstdlib>

#define USE_VR_RENDERER  0
#define USE_AUDIO_ENGINE 1

#if USE_AUDIO_ENGINE
#    include "axmol/audio/AudioEngine.h"
#endif

#if USE_VR_RENDERER && defined(AX_ENABLE_VR)
#    if defined(AX_ENABLE_OPENXR)
#        include "axmol/vr/VRSceneCompositor.h"
#    endif
#    include "axmol/vr/VRPreviewSceneCompositor.h"
#endif

using namespace ax;

static ax::Size designResolutionSize = ax::Size(1280, 720);

AppDelegate::AppDelegate() {}

AppDelegate::~AppDelegate() {}

// if you want a different context, modify the value of contextAttrs
// it will affect all platforms
void AppDelegate::applicationWillLaunch()
{
    // AXMOLUO_RHI=gl|d3d11|d3d12|vk picks the graphics backend (default: Axmol's own choice),
    // so a rendering difference can be checked against another driver.
    if (const char* rhi = std::getenv("AXMOLUO_RHI"); rhi && *rhi)
    {
        const std::string_view name(rhi);
        if (name == "gl")
            GraphicsCore::setPreferredBackend(rhi::GraphicsBackend::OpenGL);
        else if (name == "d3d11")
            GraphicsCore::setPreferredBackend(rhi::GraphicsBackend::D3D11);
        else if (name == "d3d12")
            GraphicsCore::setPreferredBackend(rhi::GraphicsBackend::D3D12);
        else if (name == "vk")
            GraphicsCore::setPreferredBackend(rhi::GraphicsBackend::Vulkan);
    }

    // Enable logging output colored text style and prefix timestamp
    setLogFmtFlag(ax::LogFmtFlag::Full);

    // Register Vulkan interop for OpenXR support, if available. This allows the engine to share Vulkan resources with
    // external APIs. if AX_ENABLE_OPENXR or AX_ENABLE_VK is not defined, this call is no-op.
    registerVulkanInterop("AxmolUO"sv);

    // set vulkan min android api level, 31 for Android 12
    // refer: https://developer.android.com/tools/releases/platforms
    GraphicsCore::setVulkanMinAndroidApiLevel(31);

    // set app context attributes: red,green,blue,alpha,depth,stencil,multisamplesCount
    // powerPreference only affect when RHI backend is D3D
    ContextAttrs contextAttrs = {.powerPreference = PowerPreference::HighPerformance};

    // V-Sync is enabled by default since axmol 2.2.
    // Uncomment to disable V-Sync and unlock FPS.
    // contextAttrs.vsync = false;

    // Enable high-DPI scaling support (non-Windows platforms only)
    // Note: cpp-tests keep the default render mode to ensure consistent performance benchmarks
#if AX_TARGET_PLATFORM != AX_PLATFORM_WIN32
    contextAttrs.renderScaleMode = RenderScaleMode::Physical;
#endif
    setContextAttrs(contextAttrs);

    // Sets preferred orientation
    const auto orientations = Device::getSupportedOrientations();
    if (bitmask::any(orientations, Device::OrientationMask::Landscape) &&
        bitmask::any(orientations, Device::OrientationMask::ReverseLandscape))
        Device::setPreferredOrientation(Device::Orientation::SensorLandscape);
    else if (bitmask::any(orientations, Device::OrientationMask::Portrait) &&
             bitmask::any(orientations, Device::OrientationMask::ReversePortrait))
        Device::setPreferredOrientation(Device::Orientation::SensorPortrait);
}

bool AppDelegate::applicationDidFinishLaunching()
{
    // initialize director
    auto director   = Director::getInstance();
    auto renderView = director->getRenderView();
    if (!renderView)
    {
#if (AX_TARGET_PLATFORM == AX_PLATFORM_WIN32) || (AX_TARGET_PLATFORM == AX_PLATFORM_MAC) || \
    (AX_TARGET_PLATFORM == AX_PLATFORM_LINUX)
        renderView = RenderView::createWithRect(
            "AxmolUO", ax::Rect(0, 0, designResolutionSize.width, designResolutionSize.height));
#else
        renderView = RenderView::create("AxmolUO");
#endif
        director->setRenderView(renderView);
    }
#if USE_VR_RENDERER && defined(AX_ENABLE_VR)
#    if defined(AX_ENABLE_OPENXR)
    // openxr is enabled, use the VRSceneCompositor for true VR rendering with headset support.
    renderView->setSceneCompositor(std::make_unique<VRSceneCompositor>());
#    else
    // For debug purposes, we can use a VR preview scene compositor to simulate VR rendering on desktop platforms.
    renderView->setSceneCompositor(std::make_unique<VRPreviewSceneCompositor>());
#    endif
#endif

    // turn on display FPS
    director->setStatsDisplay(false);

    // set FPS. the default value is 1.0/60 if you don't call this
    director->setAnimationInterval(1.0f / 60);

    // Set the design resolution
    renderView->setDesignResolutionSize(designResolutionSize.width, designResolutionSize.height,
                                        ResolutionPolicy::SHOW_ALL);

    // UO_RENDER_SMOKE=<file.png>: draw a generated map with the world renderer, save a
    // screenshot and quit. Needs no UO data; used to smoke-test rendering in CI.
    if (const char* smoke = std::getenv("UO_RENDER_SMOKE"); smoke && *smoke)
    {
        auto* scene = new uo::render::RenderSmokeScene(smoke);
        if (!scene->init())
        {
            delete scene;
            return false;
        }
        scene->autorelease();
        director->runWithScene(scene);
        return true;
    }

    // create a scene. it's an autorelease object
    auto scene = utils::createInstance<LoginScene>();

    // run
    director->runWithScene(scene);

    return true;
}

// This function will be called when the app is inactive. Note, when receiving a phone call it is invoked.
void AppDelegate::applicationDidEnterBackground()
{
    Director::getInstance()->deactivate();

#if USE_AUDIO_ENGINE
    AudioEngine::pauseAll();
#endif
}

// this function will be called when the app is active again
void AppDelegate::applicationWillEnterForeground()
{
    Director::getInstance()->activate();

#if USE_AUDIO_ENGINE
    AudioEngine::resumeAll();
#endif
}

void AppDelegate::applicationWillQuit()
{
    GameClient::instance().shutdown();
}
