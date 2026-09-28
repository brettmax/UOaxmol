// SPDX-License-Identifier: BSD-2-Clause
#include "uo/net/Session.h"

#include "uo/io/BinaryReader.h"

namespace uo::net
{

namespace
{
constexpr ClientVersion kSeedPacketVersion = makeVersion(6, 0, 5, 0);
}

Session::Session(Transport& transport, SessionListener& listener)
    : _transport(transport), _listener(listener), _table(makeVersion(7, 0, 15, 1)), _framer(_table)
{}

const char* Session::loginErrorText(std::uint8_t code)
{
    switch (code)
    {
    case 0x00: return "Incorrect name or password.";
    case 0x01: return "Someone is already using this account.";
    case 0x02: return "Your account has been blocked.";
    case 0x03: return "Your account credentials are invalid.";
    case 0x04: return "Communication problem.";
    case 0x05: return "The IGR concurrency limit has been met.";
    case 0x06: return "The IGR time limit has been met.";
    case 0x07: return "General IGR authentication failure.";
    default: return "Login failed.";
    }
}

void Session::setState(State s)
{
    _state = s;
    _listener.onStateChanged(static_cast<int>(s));
}

void Session::start(const Settings& settings)
{
    _settings = settings;
    _table    = PacketTable(settings.version);
    _framer   = PacketFramer(_table);
    _relaying = false;
    _characters.clear();
    setState(State::LoginConnecting);
    _transport.connect(settings.host, settings.port);
}

void Session::stop()
{
    _transport.disconnect();
    setState(State::Disconnected);
}

void Session::send(std::vector<std::uint8_t> packet)
{
    _transport.send(std::move(packet));
}

void Session::onConnected()
{
    if (_state == State::LoginConnecting)
    {
        if (_settings.version >= kSeedPacketVersion)
            send(out::seed(_settings.seed, _settings.version));
        else
            send(out::seedOld(_settings.seed));
        send(out::accountLogin(_settings.account, _settings.password));
        setState(State::LoginAuthenticating);
    }
    else if (_state == State::GameConnecting)
    {
        // The game server identifies the relay by the key, sent raw in place of a seed.
        send(out::seedOld(_authKey));
        send(out::gameLogin(_authKey, _settings.account, _settings.password));
        setState(State::GameAuthenticating);
    }
}

void Session::onConnectFailed(const std::string& reason)
{
    _listener.onLoginError("Could not connect: " + reason);
    setState(State::Disconnected);
}

void Session::onDisconnected()
{
    if (_relaying)
        return;  // expected: we dropped the login server ourselves
    setState(State::Disconnected);
    _listener.onDisconnected();
}

void Session::onData(std::span<const std::uint8_t> bytes)
{
    if (!_framer.feed(bytes, [this](std::span<const std::uint8_t> p) { handle(p); }))
    {
        _listener.onLoginError("Protocol error: the server sent a packet this client version does not know.");
        stop();
    }
}

void Session::selectShard(std::uint16_t index)
{
    send(out::selectShard(index));
}

void Session::selectCharacter(std::uint32_t slot)
{
    std::string name;
    for (const auto& c : _characters)
    {
        if (c.slot == slot)
            name = c.name;
    }
    send(out::selectCharacter(slot, name, _settings.seed, 0));
    setState(State::EnteringWorld);
}

void Session::handle(std::span<const std::uint8_t> packet)
{
    io::BinaryReader r(packet);
    std::uint8_t id = r.readU8();
    if (_table.length(id) < 0)
        r.skip(2);

    bool game = _state >= State::GameAuthenticating;

    switch (id)
    {
    case 0x82:  // login denied
    case 0x53:  // error popup (character creation / login problems)
        _listener.onLoginError(loginErrorText(r.readU8()));
        return;

    case 0xA8:  // shard list
    {
        r.readU8();  // flags
        std::uint16_t count = r.readU16BE();
        std::vector<ShardInfo> shards;
        for (std::uint16_t i = 0; i < count && !r.overflowed(); ++i)
        {
            ShardInfo s;
            s.index       = r.readU16BE();
            s.name        = r.readASCII(32);
            s.percentFull = r.readU8();
            s.timezone    = r.readU8();
            s.address     = r.readU32BE();
            shards.push_back(std::move(s));
        }
        setState(State::ShardList);
        _listener.onShardList(shards);
        return;
    }

    case 0x8C:  // relay to game server
    {
        std::uint8_t a = r.readU8(), b = r.readU8(), c = r.readU8(), d = r.readU8();
        std::uint16_t port = r.readU16BE();
        _authKey           = r.readU32BE();

        std::string host = _settings.host;
        if (!_settings.ignoreRelayAddress && (a | b | c | d) != 0)
            host = std::to_string(a) + "." + std::to_string(b) + "." + std::to_string(c) + "." + std::to_string(d);

        _relaying = true;
        _transport.disconnect();
        _relaying = false;

        _framer.reset();
        _framer.setCompressed(true);
        setState(State::GameConnecting);
        _transport.connect(host, port);
        return;
    }

    case 0xA9:  // character list
    case 0x86:  // character list update after a delete
    {
        std::uint8_t count = r.readU8();
        _characters.clear();
        for (std::uint32_t i = 0; i < count; ++i)
        {
            std::string name = r.readASCII(30);
            r.skip(30);  // password, unused
            if (!name.empty())
                _characters.push_back({i, name});
        }
        setState(State::CharacterList);
        _listener.onCharacterList(_characters);
        return;
    }

    case 0xBD:  // server asks for our version
        send(out::clientVersion(clientVersionToString(_settings.version)));
        return;

    case 0x1B:  // login confirm: we are in the world
        setState(State::InGame);
        break;

    default:
        break;
    }

    if (game)
        _listener.onGamePacket(packet);
}

}  // namespace uo::net
