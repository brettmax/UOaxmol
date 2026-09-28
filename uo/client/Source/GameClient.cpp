// SPDX-License-Identifier: MIT
#include "GameClient.h"

#include "uo/net/OutgoingPackets.h"

GameClient& GameClient::instance()
{
    static GameClient client;
    return client;
}

GameClient::GameClient() : _session(_transport, *this)
{
    _settings = Settings::load();
    _transport.bind(&_session);
    _world = std::make_unique<uo::game::World>(_settings.clientVersion);
}

bool GameClient::loadAssets()
{
    uo::assets::Installation::Options o;
    o.directory = _settings.uoDirectory;
    o.version   = _settings.clientVersion;
    o.maps      = {_settings.map};

    _textures.reset();
    _assetsLoaded = _install.load(o);
    _error        = _install.error();
    if (_assetsLoaded)
        _textures = std::make_unique<UOTextures>(_install);
    return _assetsLoaded;
}

void GameClient::connect()
{
    _world = std::make_unique<uo::game::World>(_settings.clientVersion);

    uo::net::Session::Settings s;
    s.host               = _settings.host;
    s.port               = _settings.port;
    s.account            = _settings.account;
    s.password           = _settings.password;
    s.version            = _settings.clientVersion;
    s.ignoreRelayAddress = _settings.ignoreRelayAddress;
    _session.start(s);
}

void GameClient::shutdown()
{
    errorHandler        = nullptr;
    shardsHandler       = nullptr;
    charactersHandler   = nullptr;
    enteredWorldHandler = nullptr;
    disconnectedHandler = nullptr;
    if (_session.state() != uo::net::Session::State::Disconnected)
        _session.stop();
    _textures.reset();
}

void GameClient::update(float dt)
{
    _transport.poll();

    // Keep-alive: ClassicUO pings roughly every 30 seconds once in game.
    if (_session.state() == uo::net::Session::State::InGame)
    {
        _pingTimer += dt;
        if (_pingTimer >= 30.f)
        {
            _pingTimer = 0;
            _session.send(uo::net::out::ping(0));
        }
    }
}

void GameClient::onLoginError(std::string message)
{
    if (errorHandler)
        errorHandler(message);
}

void GameClient::onShardList(const std::vector<uo::net::ShardInfo>& shards)
{
    if (shardsHandler)
        shardsHandler(shards);
}

void GameClient::onCharacterList(const std::vector<uo::net::CharacterSlot>& characters)
{
    if (charactersHandler)
        charactersHandler(characters);
}

void GameClient::onGamePacket(std::span<const std::uint8_t> packet)
{
    bool entering = !_world->inWorld();
    _world->handle(packet);
    if (entering && _world->inWorld() && enteredWorldHandler)
        enteredWorldHandler();
}

void GameClient::onDisconnected()
{
    if (disconnectedHandler)
        disconnectedHandler();
}
