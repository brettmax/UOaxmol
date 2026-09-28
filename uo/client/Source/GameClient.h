// SPDX-License-Identifier: MIT
#pragma once

#include "Settings.h"
#include "net/YasioTransport.h"
#include "render/TextureCache.h"

#include "uo/assets/Installation.h"
#include "uo/game/World.h"
#include "uo/net/Session.h"

#include <functional>
#include <memory>
#include <string>

// Everything that outlives a scene: settings, the loaded UO data, the connection and the
// world. Scenes register for the callbacks they care about and drop them when they exit.
// The Axmol counterpart of ClassicUO's GameController + UOFileManager + NetClient.
class GameClient final : public uo::net::SessionListener
{
public:
    static GameClient& instance();

    Settings& settings() { return _settings; }

    // Loads UO data from settings().uoDirectory. False and error() on failure.
    bool loadAssets();
    bool assetsLoaded() const { return _assetsLoaded; }
    const std::string& error() const { return _error; }

    const uo::assets::Installation& install() const { return _install; }
    UOTextures& textures() { return *_textures; }
    uo::net::Session& session() { return _session; }
    uo::game::World& world() { return *_world; }

    void connect();
    // Pumps the network. Called every frame by the running scene.
    void update(float dt);

    // Scene hooks (set/cleared by the active scene).
    std::function<void(const std::string&)> errorHandler;
    std::function<void(const std::vector<uo::net::ShardInfo>&)> shardsHandler;
    std::function<void(const std::vector<uo::net::CharacterSlot>&)> charactersHandler;
    std::function<void()> enteredWorldHandler;
    std::function<void()> disconnectedHandler;

    // SessionListener
    void onLoginError(std::string message) override;
    void onShardList(const std::vector<uo::net::ShardInfo>& shards) override;
    void onCharacterList(const std::vector<uo::net::CharacterSlot>& characters) override;
    void onGamePacket(std::span<const std::uint8_t> packet) override;
    void onDisconnected() override;

private:
    GameClient();

    Settings _settings;
    uo::assets::Installation _install;
    std::unique_ptr<UOTextures> _textures;
    bool _assetsLoaded = false;
    std::string _error;

    YasioTransport _transport;
    uo::net::Session _session;
    std::unique_ptr<uo::game::World> _world;
    float _pingTimer = 0;
};
