// SPDX-License-Identifier: BSD-2-Clause
// Part of the AxmolUO client core. Login and connection flow ported from ClassicUO's
// LoginScene and NetClient; the socket itself is abstracted so the flow can run against a
// yasio socket in the Axmol client, a WebSocket in the browser build, or a test harness.
#pragma once

#include "uo/net/OutgoingPackets.h"
#include "uo/net/PacketFramer.h"
#include "uo/net/PacketTable.h"

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace uo::net
{

class Transport
{
public:
    virtual ~Transport() = default;
    // Asynchronous: the transport calls Session::onConnected / onConnectFailed later.
    virtual void connect(const std::string& host, std::uint16_t port) = 0;
    virtual void send(std::vector<std::uint8_t> bytes)                = 0;
    virtual void disconnect()                                         = 0;
};

struct ShardInfo
{
    std::uint16_t index = 0;
    std::string name;
    std::uint8_t percentFull = 0;
    std::uint8_t timezone    = 0;
    std::uint32_t address    = 0;
};

struct CharacterSlot
{
    std::uint32_t slot = 0;
    std::string name;
};

class SessionListener
{
public:
    virtual ~SessionListener() = default;
    virtual void onStateChanged(int /*state*/) {}
    virtual void onLoginError(std::string /*message*/) {}
    virtual void onShardList(const std::vector<ShardInfo>& /*shards*/) {}
    virtual void onCharacterList(const std::vector<CharacterSlot>& /*characters*/) {}
    // Every packet once the game server connection is up (including the ones the session
    // also inspects, such as 0x1B), for the world model to consume.
    virtual void onGamePacket(std::span<const std::uint8_t> /*packet*/) {}
    virtual void onDisconnected() {}
};

class Session
{
public:
    enum class State
    {
        Disconnected,
        LoginConnecting,
        LoginAuthenticating,
        ShardList,
        GameConnecting,
        GameAuthenticating,
        CharacterList,
        EnteringWorld,
        InGame,
    };

    struct Settings
    {
        std::string host;
        std::uint16_t port = 2593;
        std::string account;
        std::string password;
        ClientVersion version = makeVersion(7, 0, 15, 1);
        // Reconnect to `host` instead of the address in 0x8C (proxies, Docker, NAT).
        bool ignoreRelayAddress = false;
        std::uint32_t seed      = 0x7F000001;
    };

    Session(Transport& transport, SessionListener& listener);

    void start(const Settings& settings);
    void selectShard(std::uint16_t index);
    void selectCharacter(std::uint32_t slot);
    void send(std::vector<std::uint8_t> packet);
    void stop();

    // Transport callbacks.
    void onConnected();
    void onConnectFailed(const std::string& reason);
    void onData(std::span<const std::uint8_t> bytes);
    void onDisconnected();

    State state() const { return _state; }
    const Settings& settings() const { return _settings; }
    const PacketTable& table() const { return _table; }
    const std::vector<CharacterSlot>& characters() const { return _characters; }

    static const char* loginErrorText(std::uint8_t code);

private:
    void setState(State s);
    void handle(std::span<const std::uint8_t> packet);

    Transport& _transport;
    SessionListener& _listener;
    Settings _settings;
    PacketTable _table;
    PacketFramer _framer;
    State _state           = State::Disconnected;
    std::uint32_t _authKey = 0;
    bool _relaying         = false;
    std::vector<CharacterSlot> _characters;
};

}  // namespace uo::net
