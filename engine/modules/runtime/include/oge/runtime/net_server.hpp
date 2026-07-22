#pragma once

#include <cstddef>
#include "oge/log.hpp"
#include "oge/ring_allocator.hpp"
#include "oge/runtime/entt.hpp"
#include "oge/runtime/net_packet_producer.hpp"
#include "oge/runtime/net_serializer.hpp"

struct _ENetPeer;
struct _ENetHost;
typedef _ENetPeer ENetPeer;
typedef _ENetHost ENetHost;
namespace oge::runtime
{

struct OnServerReceiveConnect
{
    ENetPeer* peer;
};

struct OnServerReceiveDisconnect
{
    ENetPeer* peer;
};

struct OnServerReceivePacket
{
    ENetPeer* peer;
    net::Buffer data;
};

class NetServer
{
   public:
    NetServer(size_t sendBufferSize) : sendPacketProducer(sendBufferSize) {}
    ~NetServer() { Shutdown(); }

    bool Initialize(uint16_t port, size_t maxClients, size_t channelCount = 2);

    void Poll(entt::dispatcher& dispatcher, uint32_t timeoutMs = 0);

    void Shutdown();

    net::Buffer StartPacket(size_t size);

    void SendReliable(ENetPeer* peer, net::Buffer data,
                      uint8_t channel = 0);

    void SendUnreliable(ENetPeer* peer, net::Buffer data,
                        uint8_t channel = 1);

   private:
    void OnClientConnected(ENetPeer* peer) { LOG_INFO("Client connected"); }

    void OnClientDisconnected(ENetPeer* peer)
    {
        LOG_INFO("Client disconnected");
    }

    void OnPacketReceived(ENetPeer* peer, uint8_t* data, size_t length)
    {
        LOG_INFO("Server received {} bytes", length);
    }

   private:
    ENetHost* host = nullptr;

    NetPacketProducer sendPacketProducer;
};
}  // namespace oge::runtime
