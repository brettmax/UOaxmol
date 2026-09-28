// SPDX-License-Identifier: MIT
#include "GameClient.h"

#include "axmol/TextSystem.h"
#include "axmol/axmol.h"

#include "uo/net/OutgoingPackets.h"

#include <algorithm>

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
    // Gumps, movement and targeting hold references into the old world; drop them first.
    _gumps.reset();
    _targeting.reset();
    _movement.reset();
    _tiles.reset();

    _world = std::make_unique<uo::world::World>(_settings.clientVersion);
    _world->setListener(this);
    _world->setRequests(this);
    _world->setClilocs(this);

    auto send = [this](std::span<const std::uint8_t> bytes) { _session.send({bytes.begin(), bytes.end()}); };

    _tiles    = std::make_unique<uo::movement::WorldTileSource>(*_world, _install.tileData());
    _tilesMap = -1;
    _movement = std::make_unique<uo::movement::MovementSystem>(*_world, *_tiles, send);
    _movement->install(_handlers);

    uo::world::TargetingHooks hooks;
    hooks.send              = send;
    hooks.cancelDoubleClick = [this] {
        if (cancelDoubleClickHandler)
            cancelDoubleClickHandler();
    };
    _targeting = std::make_unique<uo::world::Targeting>(*_world, std::move(hooks));
    _targeting->install(_handlers);

    if (_assetsLoaded)
    {
        _gumps = std::make_unique<uo::client::gumps::GumpSystem>(
            _install, *_world, [this](std::vector<std::uint8_t> bytes) { _session.send(std::move(bytes)); });
        _gumps->install(_handlers);
        _gumps->setText(_gumpText.get());
    }

    walkHandler = [this](uo::world::Direction dir, bool run) {
        return _movement->walk(dir, run).kind != uo::movement::WalkResult::Kind::Rejected;
    };
    playerTeleportedHandler = [this](uo::world::Player&) { _movement->onPlayerTeleported(); };
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
    {
        _textures = std::make_unique<UOTextures>(_install);
        // Missing fonts.mul is not fatal yet: scenes fall back to TTF labels.
        if (_gumps)
            _gumps->setText(nullptr);
        _gumpText.reset();
        if (uo::client::text::TextSystem::instance().init(_install))
            _gumpText = std::make_unique<uo::client::gumps::UoFontGumpText>(_install);
        else
            AXLOGW("AxmolUO: fonts.mul not found; UO fonts disabled");
        if (_gumps)
            _gumps->setText(_gumpText.get());
        _audio.shutdown();
        _audio.initialize(_install.sounds());
        _audio.setClientVersion(_settings.clientVersion);
    }
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
    _transport.shutdown();
    _gumps.reset();
    _audio.shutdown();
    uo::client::text::TextSystem::instance().shutdown();
    _textures.reset();
}

void GameClient::update(float dt)
{
    _transport.poll();
    _audio.update();

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

void GameClient::updateMovement(const std::optional<uo::movement::MovementIntent>& intent, float dt)
{
    // The facet can change under the player (0xBF 0x08); statics are cached per facet.
    const int mapIndex = std::max(0, _world->mapIndex);
    if (mapIndex != _tilesMap)
    {
        _tilesMap = mapIndex;
        _tiles->setMap(_install.map(mapIndex));
    }

    _tiles->rebuild();
    _movement->update(intent, static_cast<int>(dt * 1000.f));
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
    if (_gumps)
        _gumps->onEntityRemoved(serial);
    if (entityRemovedHandler)
        entityRemovedHandler(serial);
}

void GameClient::onNameChanged(uo::world::Entity& e)
{
    if (_gumps)
        _gumps->onNameChanged(e);
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

void GameClient::onSound(const uo::world::SoundRequest& sound)
{
    uo::audio::Listener listener;
    if (const uo::world::Player* player = _world->player())
    {
        listener.inGame = true;
        listener.x      = player->x;
        listener.y      = player->y;
    }
    _audio.playSoundAt(sound.index, sound.x, sound.y, listener);
}

void GameClient::onMusic(int index)
{
    if (index < 0)
        _audio.stopMusic();
    else
        _audio.playMusic(index);
}

// --- WorldListener: gumps --------------------------------------------------------------------

void GameClient::onContainerContentsChanged(uo::world::Serial container)
{
    if (_gumps)
        _gumps->onContainerContentsChanged(container);
}

void GameClient::onEquipmentChanged(uo::world::Serial mobile)
{
    if (_gumps)
        _gumps->onEquipmentChanged(mobile);
}

void GameClient::onStatsChanged(uo::world::Entity& e)
{
    if (_gumps)
        _gumps->onStatsChanged(e);
}

void GameClient::onSkillsChanged(int skillIndex, bool openWindow)
{
    if (_gumps)
        _gumps->onSkillsChanged(skillIndex, openWindow);
}

void GameClient::onOpenContainer(uo::world::Item& container, std::uint16_t gumpGraphic)
{
    if (_gumps)
        _gumps->onOpenContainer(container, gumpGraphic);
}

void GameClient::onOpenPaperdoll(uo::world::Mobile& mobile, const std::string& title, bool canLift)
{
    if (_gumps)
        _gumps->onOpenPaperdoll(mobile, title, canLift);
}

void GameClient::onCloseServerGump(uo::world::Serial gumpSerial, std::uint32_t button)
{
    if (_gumps)
        _gumps->onCloseServerGump(gumpSerial, button);
}

void GameClient::onCloseUi(uo::world::CloseUiKind kind, uo::world::Serial serial)
{
    if (_gumps)
        _gumps->onCloseUi(static_cast<std::uint32_t>(kind), serial);
}

void GameClient::onDragEnded()
{
    if (_gumps)
        _gumps->onDragEnded();
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
