#pragma once

#include <cstddef>

#include "oge/log.hpp"
#include "oge/runtime/entt.hpp"
#include "oge/runtime/net_packet_sender.hpp"
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
    uint32_t peerId;
};

struct OnServerReceiveDisconnect
{
    ENetPeer* peer;
    uint32_t peerId;
};

struct OnServerReceivePacket
{
    ENetPeer* peer;
    uint32_t peerId;
    net::Buffer* data;
};

class NetServer : public NetPacketSender
{
   public:
    NetServer()
    {
    }
    ~NetServer()
    {
        Shutdown();
    }

    bool Initialize(
        uint16_t port, size_t maxClients, size_t channelCount = 2,
        std::pmr::memory_resource* memory = std::pmr::new_delete_resource());

    void Poll(entt::dispatcher& dispatcher, float dt, uint32_t timeoutMs = 0);

    void Shutdown();

    void Disconnect(ENetPeer* peer, uint32_t signal = 0);

   private:
    void OnClientConnected(ENetPeer* peer);

    void OnClientDisconnected(ENetPeer* peer);

    void OnPacketReceived(ENetPeer* peer, uint8_t* data, size_t length);
};
}  // namespace oge::runtime
