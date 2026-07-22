#pragma once

#include "oge/log.hpp"
#include "oge/runtime/entt.hpp"

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
    uint8_t* data;
    size_t length;
};

class NetServer
{
   public:
    ~NetServer() { Shutdown(); }

    bool Initialize(uint16_t port, size_t maxClients, size_t channelCount = 2);

    void Poll(entt::dispatcher& dispatcher, uint32_t timeoutMs = 0);

    void Shutdown();

    void SendReliable(ENetPeer* peer, const void* data, size_t size, uint8_t channel = 0);

    void SendUnreliable(ENetPeer* peer, const void* data, size_t size, uint8_t channel = 1);

   private:
    void OnClientConnected(ENetPeer* peer) { LOG_INFO("Client connected"); }

    void OnClientDisconnected(ENetPeer* peer) { LOG_INFO("Client disconnected"); }

    void OnPacketReceived(ENetPeer* peer, uint8_t* data, size_t length)
    {
        LOG_INFO("Server received {} bytes", length);
    }

   private:
    ENetHost* host = nullptr;
};
}  // namespace OneGame::Engine
