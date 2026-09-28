// SPDX-License-Identifier: MIT
#pragma once

#include "uo/net/Session.h"

namespace yasio
{
inline namespace inet
{
class io_service;
class io_transport;
}  // namespace inet
}  // namespace yasio

// uo::net::Transport over yasio, the socket library bundled with Axmol. Network events are
// queued by yasio's IO thread and delivered on the game thread from poll(), so the Session and
// World are only ever touched from the main loop (the same rule ClassicUO follows).
//
// The login and game servers use separate channels: closing the login socket is asynchronous,
// and its close event must not be mistaken for losing the game connection.
class YasioTransport final : public uo::net::Transport
{
public:
    YasioTransport();
    ~YasioTransport() override;

    void bind(uo::net::Session* session) { _session = session; }

    void connect(const std::string& host, std::uint16_t port) override;
    void send(std::vector<std::uint8_t> bytes) override;
    void disconnect() override;

    // Delivers queued network events. Call once per frame.
    void poll();

private:
    yasio::io_service* _service   = nullptr;
    yasio::io_transport* _current = nullptr;
    int _channel                  = -1;  // channel whose events we listen to
    int _nextChannel              = 0;
    uo::net::Session* _session    = nullptr;
};
