#pragma once
#include <enet/enet.h>

#include "Engine/entt.hpp"
#include "Engine/Logger.hpp"

namespace OneGame::Engine
{
class NetClient
{
public:
    NetClient(entt::dispatcher& events) : m_events(events) {}

    ~NetClient()
    {
        Shutdown();
    }

    bool Initialize(size_t channelCount = 2)
    {
        if (enet_initialize() != 0)
        {
            LOG_ERROR("ENet init failed");
            return false;
        }

        host = enet_host_create(
            nullptr,
            1,
            channelCount,
            0,
            0);

        if (!host)
        {
            LOG_ERROR("Failed to create client host");
            return false;
        }

        return true;
    }

    bool Connect(const char* ip,
                 uint16_t port,
                 uint32_t timeoutMs = 5000)
    {
        if (!host) return false;

        ENetAddress address;
        enet_address_set_host(&address, ip);
        address.port = port;

        peer = enet_host_connect(host, &address, 2, 0);

        if (!peer)
        {
            LOG_ERROR("Connection failed");
            return false;
        }

        ENetEvent event;

        if (enet_host_service(host, &event, timeoutMs) > 0 &&
            event.type == ENET_EVENT_TYPE_CONNECT)
        {
            LOG_INFO("Connected to server");
            return true;
        }

        enet_peer_reset(peer);
        peer = nullptr;
        LOG_INFO("Connection timeout");
        return false;
    }

    void Poll(uint32_t timeoutMs = 0)
    {
        if (!host) return;

        ENetEvent event;

        while (enet_host_service(host, &event, timeoutMs) > 0)
        {
            switch (event.type)
            {
                case ENET_EVENT_TYPE_RECEIVE:
                    OnPacketReceived(event.packet->data,
                                     event.packet->dataLength);
                    enet_packet_destroy(event.packet);
                    break;

                case ENET_EVENT_TYPE_DISCONNECT:
                    LOG_INFO("Disconnected from server");
                    peer = nullptr;
                    break;

                default:
                    break;
            }
        }
    }

    void Disconnect(uint32_t timeoutMs = 3000)
    {
        if (!peer) return;

        enet_peer_disconnect(peer, 0);

        ENetEvent event;

        while (enet_host_service(host, &event, timeoutMs) > 0)
        {
            if (event.type == ENET_EVENT_TYPE_DISCONNECT)
                break;

            if (event.type == ENET_EVENT_TYPE_RECEIVE)
                enet_packet_destroy(event.packet);
        }

        enet_peer_reset(peer);
        peer = nullptr;
    }

    void Shutdown()
    {
        if (host)
        {
            enet_host_destroy(host);
            host = nullptr;
            enet_deinitialize();
            LOG_INFO("Client shutdown");
        }
    }

    void SendReliable(const void* data,
                      size_t size,
                      uint8_t channel = 0)
    {
        if (!peer) return;

        ENetPacket* packet = enet_packet_create(
            data,
            size,
            ENET_PACKET_FLAG_RELIABLE);

        enet_peer_send(peer, channel, packet);
    }

    void SendUnreliable(const void* data,
                        size_t size,
                        uint8_t channel = 1)
    {
        if (!peer) return;

        ENetPacket* packet = enet_packet_create(
            data,
            size,
            0);

        enet_peer_send(peer, channel, packet);
    }

private:

    void OnPacketReceived(uint8_t* data, size_t length)
    {
        LOG_INFO("Client received {} bytes", length);
    }

private:
    ENetHost* host = nullptr;
    ENetPeer* peer = nullptr;

    entt::dispatcher& m_events;
};
}
