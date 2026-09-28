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
    resetWorld();
}

void GameClient::resetWorld()
{
    _world = std::make_unique<uo::world::World>(_settings.clientVersion);
    _world->setListener(this);
    _world->setRequests(this);
    _world->setClilocs(this);
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
    resetWorld();

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
    _handlers.handle(*_world, packet, _session.table());
}

void GameClient::onDisconnected()
{
    if (disconnectedHandler)
        disconnectedHandler();
}

// --- WorldListener ---------------------------------------------------------------------------

void GameClient::onEnterWorld(uo::world::Player&)
{
    if (enteredWorldHandler)
        enteredWorldHandler();
}

void GameClient::onEntityCreated(uo::world::Entity& e)
{
    if (entityUpdatedHandler)
        entityUpdatedHandler(e);
}

void GameClient::onEntityUpdated(uo::world::Entity& e)
{
    if (entityUpdatedHandler)
        entityUpdatedHandler(e);
}

void GameClient::onEntityRemoved(uo::world::Serial serial, uo::world::EntityKind)
{
    if (entityRemovedHandler)
        entityRemovedHandler(serial);
}

void GameClient::onNameChanged(uo::world::Entity& e)
{
    if (entityUpdatedHandler)
        entityUpdatedHandler(e);
}

void GameClient::onPlayerTeleported(uo::world::Player& player)
{
    if (playerTeleportedHandler)
        playerTeleportedHandler(player);
}

void GameClient::onMessage(const uo::world::Message& msg)
{
    if (messageHandler)
        messageHandler(msg);
}

// --- ServerRequests --------------------------------------------------------------------------

void GameClient::requestMobileStatus(uo::world::Serial serial)
{
    _session.send(uo::net::out::statusRequest(serial));
}

void GameClient::singleClick(uo::world::Serial serial)
{
    _session.send(uo::net::out::singleClick(serial));
}

void GameClient::doubleClick(uo::world::Serial serial)
{
    _session.send(uo::net::out::doubleClick(serial));
}

// --- ClilocResolver --------------------------------------------------------------------------

std::string GameClient::get(std::uint32_t cliloc)
{
    if (!_assetsLoaded)
        return {};
    const std::string* s = _install.cliloc().get(static_cast<std::int32_t>(cliloc));
    return s ? *s : std::string();
}

std::optional<std::string> GameClient::translate(std::uint32_t cliloc, std::string_view args, bool capitalize)
{
    if (!_assetsLoaded || !_install.cliloc().get(static_cast<std::int32_t>(cliloc)))
        return std::nullopt;
    std::string text = _install.cliloc().format(static_cast<std::int32_t>(cliloc), args);
    if (capitalize && !text.empty() && text[0] >= 'a' && text[0] <= 'z')
        text[0] = static_cast<char>(text[0] - 'a' + 'A');
    return text;
}
