#pragma once

#include "Engine/entt.hpp"
#include "Engine/Logger.hpp"

struct _ENetPeer;
struct _ENetHost;
typedef _ENetPeer ENetPeer;
typedef _ENetHost ENetHost;
namespace OneGame::Engine
{

struct OnDisconnectedFromServer
{
};

struct OnClientPacketReceived
{
    uint8_t* data;
    size_t length;
};

class NetClient
{
public:
    ~NetClient()
    {
        Shutdown();
    }

    bool Initialize(size_t channelCount = 2);

    bool Connect(const char* ip,
                 uint16_t port,
                 uint32_t timeoutMs = 5000);

    void Poll(entt::dispatcher& dispatcher, uint32_t timeoutMs = 0);

    void Disconnect(uint32_t timeoutMs = 3000);

    void Shutdown();

    void SendReliable(const void* data,
                      size_t size,
                      uint8_t channel = 0);

    void SendUnreliable(const void* data,
                        size_t size,
                        uint8_t channel = 1);

private:

    void OnPacketReceived(uint8_t* data, size_t length)
    {
        LOG_INFO("Client received {} bytes", length);
    }

private:
    ENetHost* host = nullptr;
    ENetPeer* peer = nullptr;
};
}
