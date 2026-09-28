// SPDX-License-Identifier: MIT
#include "YasioTransport.h"

#include "yasio/yasio.hpp"

using namespace yasio;

YasioTransport::YasioTransport()
{
    _service = new io_service(2);
    _service->set_option(YOPT_S_NO_DISPATCH, 1);
    _service->set_option(YOPT_S_DNS_QUERIES_TIMEOUT, 5);
    // No framing at this layer: UO packet lengths depend on the id and on compression, so the
    // raw stream goes to uo::net::PacketFramer.
    _service->start([this](event_ptr&& ev) {
        if (!_session || ev->cindex() != _channel)
            return;  // a late event from the channel we already left

        switch (ev->kind())
        {
        case YEK_ON_OPEN:
            if (ev->status() == 0)
            {
                _current = ev->transport();
                _session->onConnected();
            }
            else
            {
                _session->onConnectFailed("error " + std::to_string(ev->status()));
            }
            break;
        case YEK_ON_PACKET:
        {
            auto& pkt = ev->packet();
            _session->onData(std::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t*>(pkt.data()), pkt.size()));
            break;
        }
        case YEK_ON_CLOSE:
            _current = nullptr;
            _session->onDisconnected();
            break;
        default:
            break;
        }
    });
}

YasioTransport::~YasioTransport()
{
    shutdown();
}

void YasioTransport::shutdown()
{
    _session = nullptr;
    _current = nullptr;
    _channel = -1;
    if (!_service)
        return;
    _service->stop();
    delete _service;
    _service = nullptr;
}

void YasioTransport::connect(const std::string& host, std::uint16_t port)
{
    if (!_service)
        return;
    _channel = _nextChannel;
    _nextChannel ^= 1;
    _current = nullptr;
    _service->set_option(YOPT_C_REMOTE_ENDPOINT, _channel, host.c_str(), static_cast<int>(port));
    _service->open(_channel, YCK_TCP_CLIENT);
}

void YasioTransport::send(std::vector<std::uint8_t> bytes)
{
    if (!_service)
        return;
    if (_current)
        _service->write(_current, bytes.data(), bytes.size());
}

void YasioTransport::disconnect()
{
    if (!_service)
        return;
    if (_channel >= 0)
        _service->close(_channel);
    _current = nullptr;
    // Leave the channel so its asynchronous close event is ignored; Session tracks the state.
    _channel = -1;
}

void YasioTransport::poll()
{
    if (!_service)
        return;
    _service->dispatch(128);
}
