// SPDX-License-Identifier: MIT
#pragma once

#include "Settings.h"
#include "net/YasioTransport.h"
#include "render/TextureCache.h"

#include "uo/assets/Installation.h"
#include "uo/movement/MovementSystem.h"
#include "uo/net/Session.h"
#include "uo/world/PacketHandlers.h"
#include "uo/world/Targeting.h"
#include "uo/world/World.h"

#include <functional>
#include <memory>
#include <string>

// Everything that outlives a scene: settings, the loaded UO data, the connection and the
// world. Scenes register for the callbacks they care about and drop them when they exit.
// The Axmol counterpart of ClassicUO's GameController + UOFileManager + NetClient.
class GameClient final : public uo::net::SessionListener,
                         public uo::world::WorldListener,
                         public uo::world::ServerRequests,
                         public uo::world::ClilocResolver
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
    uo::world::World& world() { return *_world; }
    // Packet dispatch into world(). Other modules (gumps, movement) register hooks here.
    uo::world::PacketHandlers& packetHandlers() { return _handlers; }
    // Walker, auto-walk and mobile step advance over world(); recreated with it on connect.
    uo::movement::MovementSystem& movement() { return *_movement; }
    // The target cursor over world().target; recreated with it on connect.
    uo::world::Targeting& targeting() { return *_targeting; }

    void connect();
    // Pumps the network. Called every frame by the running scene.
    void update(float dt);
    // Advances movement one frame: auto-walk, then `intent` (the scene's input), then every
    // mobile's steps. Called by the world scene after update().
    void updateMovement(const std::optional<uo::movement::MovementIntent>& intent, float dt);

    // Scene hooks (set/cleared by the active scene).
    std::function<void(const std::string&)> errorHandler;
    std::function<void(const std::vector<uo::net::ShardInfo>&)> shardsHandler;
    std::function<void(const std::vector<uo::net::CharacterSlot>&)> charactersHandler;
    std::function<void()> enteredWorldHandler;
    std::function<void()> disconnectedHandler;
    std::function<void(const uo::world::Entity&)> entityUpdatedHandler;
    std::function<void(uo::world::Serial)> entityRemovedHandler;
    std::function<void(const uo::world::Message&)> messageHandler;
    // Movement: sends a walk request for the player (uo::movement::Walker); false when refused.
    std::function<bool(uo::world::Direction, bool run)> walkHandler;
    // Movement: the server moved the player (0x20 / 0xF3); the walker must resync.
    std::function<void(uo::world::Player&)> playerTeleportedHandler;
    // Targeting: a click was used by the target cursor, so it must not become a double-click.
    std::function<void()> cancelDoubleClickHandler;

    // SessionListener
    void onLoginError(std::string message) override;
    void onShardList(const std::vector<uo::net::ShardInfo>& shards) override;
    void onCharacterList(const std::vector<uo::net::CharacterSlot>& characters) override;
    void onGamePacket(std::span<const std::uint8_t> packet) override;
    void onDisconnected() override;

    // WorldListener
    void onEnterWorld(uo::world::Player& player) override;
    void onEntityCreated(uo::world::Entity& e) override;
    void onEntityUpdated(uo::world::Entity& e) override;
    void onEntityRemoved(uo::world::Serial serial, uo::world::EntityKind kind) override;
    void onNameChanged(uo::world::Entity& e) override;
    void onPlayerTeleported(uo::world::Player& player) override;
    void onMessage(const uo::world::Message& msg) override;

    // ServerRequests (0xBD is answered by the session)
    void requestMobileStatus(uo::world::Serial serial) override;
    void singleClick(uo::world::Serial serial) override;
    void doubleClick(uo::world::Serial serial) override;

    // ClilocResolver, over the installation's cliloc table
    std::string get(std::uint32_t cliloc) override;
    std::optional<std::string> translate(std::uint32_t cliloc, std::string_view args, bool capitalize) override;

private:
    GameClient();

    Settings _settings;
    uo::assets::Installation _install;
    std::unique_ptr<UOTextures> _textures;
    bool _assetsLoaded = false;
    std::string _error;

    YasioTransport _transport;
    uo::net::Session _session;
    void resetWorld();

    std::unique_ptr<uo::world::World> _world;
    uo::world::PacketHandlers _handlers;
    std::unique_ptr<uo::movement::WorldTileSource> _tiles;
    std::unique_ptr<uo::movement::MovementSystem> _movement;
    std::unique_ptr<uo::world::Targeting> _targeting;
    int _tilesMap = -1;  // facet _tiles reads, or -1 before one is set
    float _pingTimer = 0;
};
