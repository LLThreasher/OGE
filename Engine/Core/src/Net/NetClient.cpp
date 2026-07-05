#include <enet/enet.h>

#include "Engine/ECS/NetClient.hpp"

using namespace OneGame::Engine;

bool NetClient::Initialize(size_t channelCount)
{
    if (enet_initialize() != 0)
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

    LOG_INFO("connecting to host {}.{}.{}.{} ({}) at port {}", address.host & 0xFF, (address.host >> 8) & 0xFF, (address.host >> 16) & 0xFF, address.host >> 24, ip, port);

    peer = enet_host_connect(host, &address, 2, 0);

    if (!peer)
    {
        LOG_ERROR("Connection failed");
        return false;
    }

    status = ClientStatus::Connecting;

    connectWaitTime = timeoutMs;
    return true;
}

void NetClient::Poll(entt::dispatcher& dispatcher, float dt, uint32_t timeoutMs)
{
    if (!host) return;

    if (status == ClientStatus::Connecting && connectWaitTime < 0)
    {
        enet_peer_reset(peer);
        peer = nullptr;
        LOG_INFO("Connection timeout");
        status = ClientStatus::Disconnected;
    }
    
    connectWaitTime -= dt * 1000.f;

    ENetEvent event;

    while (enet_host_service(host, &event, timeoutMs) > 0)
    {
        switch (event.type)
        {
            case ENET_EVENT_TYPE_CONNECT:
                LOG_INFO("Connected to server");
                status = ClientStatus::Connected;
                break;

            case ENET_EVENT_TYPE_RECEIVE:
                OnPacketReceived(event.packet->data, event.packet->dataLength);
                dispatcher.trigger<OnClientPacketReceived>({event.packet->data, event.packet->dataLength});
                enet_packet_destroy(event.packet);
                break;

            case ENET_EVENT_TYPE_DISCONNECT:
                LOG_INFO("Disconnected from server");
                peer = nullptr;
                status = ClientStatus::Disconnected;
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

    status = ClientStatus::Disconnecting;

    enet_peer_reset(peer);
    peer = nullptr;
}

void NetClient::Shutdown()
{
    if (host)
    {
        enet_host_destroy(host);
        host = nullptr;
        enet_deinitialize();
        LOG_INFO("Client shutdown");
    }
}

void NetClient::SendReliable(const void* data, size_t size, uint8_t channel)
{
    if (!peer) return;

    ENetPacket* packet = enet_packet_create(data, size, ENET_PACKET_FLAG_RELIABLE);

    enet_peer_send(peer, channel, packet);
}

void NetClient::SendUnreliable(const void* data, size_t size, uint8_t channel)
{
    if (!peer) return;

    ENetPacket* packet = enet_packet_create(data, size, 0);

    enet_peer_send(peer, channel, packet);
}
