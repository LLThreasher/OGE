#include "oge/runtime/net_server.hpp"

#include <enet/enet.h>

#include <memory_resource>

#include "enet_interface.hpp"
#include "oge/runtime/net_serializer.hpp"

namespace oge::runtime
{
bool NetServer::Initialize(uint16_t port, size_t maxClients,
                           size_t channelCount,
                           std::pmr::memory_resource* memory)
{
    if (oge_enet_initialize(memory) != 0)
    {
        LOG_ERROR("ENet init failed");
        return false;
    }

    ENetAddress address;
    address.host = ENET_HOST_ANY;
    address.port = port;

    host = enet_host_create(&address, maxClients, channelCount, 0, 0);

    if (!host)
    {
        LOG_ERROR("Failed to create server host");
        return false;
    }

    LOG_INFO("Server initialized on port {}", port);

    return true;
}

void NetServer::Poll(entt::dispatcher& dispatcher, uint32_t timeoutMs)
{
    if (!host) return;

    ENetEvent event;

    while (enet_host_service(host, &event, timeoutMs) > 0)
    {
        switch (event.type)
        {
            case ENET_EVENT_TYPE_CONNECT:
                OnClientConnected(event.peer);
                dispatcher.trigger<OnServerReceiveConnect>(
                    {event.peer, event.peer->connectID});
                break;

            case ENET_EVENT_TYPE_RECEIVE:
                OnPacketReceived(event.peer, event.packet->data,
                                 event.packet->dataLength);
                dispatcher.trigger<OnServerReceivePacket>(
                    {event.peer, event.peer->connectID,
                     net::Buffer{event.packet->data, event.packet->dataLength}
                         .ToReadOnly()});
                enet_packet_destroy(event.packet);
                break;

            case ENET_EVENT_TYPE_DISCONNECT:
                OnClientDisconnected(event.peer);
                dispatcher.trigger<OnServerReceiveDisconnect>(
                    {event.peer, event.peer->connectID});
                break;

            default:
                break;
        }
    }
}

void NetServer::Shutdown()
{
    if (host)
    {
        enet_host_destroy(host);
        host = nullptr;
        oge_enet_shutdown();
        LOG_INFO("Server shutdown");
    }
}
}  // namespace oge::runtime
