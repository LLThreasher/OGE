#include "oge/runtime/net_client.hpp"

#include <enet/enet.h>

#include <cstddef>
#include <memory_resource>

#include "enet_interface.hpp"
#include "oge/runtime/net_packet_sender.hpp"
#include "oge/runtime/net_serializer.hpp"

using namespace oge::runtime;

bool NetClient::Initialize(size_t channelCount,
                           std::pmr::memory_resource* memory)
{
    if (oge_enet_initialize(memory) != 0)
    {
        LOG_ERROR("ENet init failed");
        return false;
    }

    host = enet_host_create(nullptr, 1, channelCount, 0, 0);

    if (!host)
    {
        LOG_ERROR("Failed to create client host");
        return false;
    }

    return true;
}

bool NetClient::Connect(const char* ip, uint16_t port, uint32_t timeoutMs)
{
    if (!host) return false;

    ENetAddress address;
    enet_address_set_host(&address, ip);
    address.port = port;

    LOG_INFO("connecting to host {}.{}.{}.{} ({}) at port {}",
             address.host & 0xFF, (address.host >> 8) & 0xFF,
             (address.host >> 16) & 0xFF, address.host >> 24, ip, port);

    peer = enet_host_connect(host, &address, 2, 0);

    if (!peer)
    {
        LOG_ERROR("Connection failed");
        return false;
    }

    state = State::Connecting;

    connectWaitTime = timeoutMs;
    return true;
}

void NetClient::Poll(entt::dispatcher& dispatcher, float dt, uint32_t timeoutMs)
{
    UpdatePacketStats(dt);
    
    if (!host) return;

    if (state == State::Connecting && connectWaitTime < 0)
    {
        enet_peer_reset(peer);
        peer = nullptr;
        LOG_INFO("Connection timeout");
        state = State::Disconnected;
        dispatcher.trigger<OnClientDisconnected>();
    }

    connectWaitTime -= dt * 1000.f;

    ENetEvent event;

    while (enet_host_service(host, &event, timeoutMs) > 0)
    {
        switch (event.type)
        {
            case ENET_EVENT_TYPE_CONNECT:
                LOG_INFO("Connected to server");
                state = State::Connected;
                dispatcher.trigger<OnClientConnected>();
                break;

            case ENET_EVENT_TYPE_RECEIVE:
            {
                OnPacketReceived(event.packet->data, event.packet->dataLength);
                auto buffer =
                    net::Buffer(event.packet->data, event.packet->dataLength).ToReadOnly();
                dispatcher.trigger<OnClientReceivePacket>(
                    {&buffer});
                enet_packet_destroy(event.packet);
                break;
            }
            case ENET_EVENT_TYPE_DISCONNECT:
                LOG_INFO("Disconnected from server");
                peer = nullptr;
                state = State::Disconnected;
                break;

            default:
                break;
        }
    }
}

void NetClient::Disconnect(uint32_t timeoutMs)
{
    if (!peer) return;

    enet_peer_disconnect(peer, 0);

    state = State::Disconnecting;

    enet_peer_reset(peer);
    peer = nullptr;
}

void NetClient::Shutdown()
{
    if (host)
    {
        Disconnect(200);
        enet_host_destroy(host);
        host = nullptr;
        oge_enet_shutdown();
        LOG_INFO("Client shutdown");
    }
}

void NetClient::Send(net::Buffer data, SendType sendType, uint8_t channel)
{
    NetPacketSender::Send(peer, data, sendType, channel);
}
