#pragma once

#include "Engine/entt.hpp"
#include "Engine/Logger.hpp"

struct _ENetPeer;
struct _ENetHost;
typedef _ENetPeer ENetPeer;
typedef _ENetHost ENetHost;
namespace OneGame::Engine
{

enum class ClientStatus
{
    Connecting,
    ConnectFailed,
    Connected,
    Disconnecting,
    Disconnected,
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
    
    ClientStatus Status() {return status;}

    void Poll(entt::dispatcher& dispatcher, float dt, uint32_t timeoutMs = 0);

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
    float connectWaitTime;
    ClientStatus status;
    ENetHost* host = nullptr;
    ENetPeer* peer = nullptr;
};
}
