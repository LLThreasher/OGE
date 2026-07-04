#pragma once
#include <enet/enet.h>

#include "Engine/Logger.hpp"

namespace OneGame::Engine
{
class NetServer
{
public:
    NetServer(entt::dispatcher& events) : m_events(events) {}
    
    ~NetServer()
    {
        Shutdown();
    }

    bool Initialize(uint16_t port,
                    size_t maxClients,
                    size_t channelCount = 2)
    {
        if (enet_initialize() != 0)
        {
            LOG_ERROR("ENet init failed");
            return false;
        }

        ENetAddress address;
        address.host = ENET_HOST_ANY;
        address.port = port;

        host = enet_host_create(
            &address,
            maxClients,
            channelCount,
            0,
            0);

        if (!host)
        {
            LOG_ERROR("Failed to create server host");
            return false;
        }

        LOG_INFO("Server initialized on port {}", port);

        return true;
    }

    void Poll(uint32_t timeoutMs = 0)
    {
        if (!host) return;

        ENetEvent event;

        while (enet_host_service(host, &event, timeoutMs) > 0)
        {
            switch (event.type)
            {
                case ENET_EVENT_TYPE_CONNECT:
                    OnClientConnected(event.peer);
                    break;

                case ENET_EVENT_TYPE_RECEIVE:
                    OnPacketReceived(event.peer,
                                     event.packet->data,
                                     event.packet->dataLength);

                    enet_packet_destroy(event.packet);
                    break;

                case ENET_EVENT_TYPE_DISCONNECT:
                    OnClientDisconnected(event.peer);
                    break;

                default:
                    break;
            }
        }
    }

    void Shutdown()
    {
        if (host)
        {
            enet_host_destroy(host);
            host = nullptr;
            enet_deinitialize();
            LOG_INFO("Server shutdown");
        }
    }

    void SendReliable(ENetPeer* peer,
                      const void* data,
                      size_t size,
                      uint8_t channel = 0)
    {
        ENetPacket* packet = enet_packet_create(
            data,
            size,
            ENET_PACKET_FLAG_RELIABLE);

        enet_peer_send(peer, channel, packet);
    }

    void SendUnreliable(ENetPeer* peer,
                        const void* data,
                        size_t size,
                        uint8_t channel = 1)
    {
        ENetPacket* packet = enet_packet_create(
            data,
            size,
            0);

        enet_peer_send(peer, channel, packet);
    }

private:

    void OnClientConnected(ENetPeer* peer)
    {
        LOG_INFO("Client connected");
    }

    void OnClientDisconnected(ENetPeer* peer)
    {
        LOG_INFO("Client disconnected");
    }

    void OnPacketReceived(ENetPeer* peer,
                          uint8_t* data,
                          size_t length)
    {
        LOG_INFO("Server received {} bytes", length);
    }

private:
    ENetHost* host = nullptr;
    entt::dispatcher& m_events;
};
}
