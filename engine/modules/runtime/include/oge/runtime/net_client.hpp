#pragma once

#include <cstddef>
#include <memory_resource>

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

enum class ClientStatus
{
    Connecting,
    Connected,
    Disconnecting,
    Disconnected,
};

struct OnClientConnectionTimeout
{
};

struct OnClientConnected
{
};

struct OnClientDisconnected
{
};

struct OnClientReceivePacket
{
    net::Buffer data;
};

class NetClient : protected NetPacketSender
{
   public:
    NetClient() {}
    ~NetClient() { Shutdown(); }

    bool Initialize(
        size_t channelCount = 2,
        std::pmr::memory_resource* memory = std::pmr::new_delete_resource());

    bool Connect(const char* ip, uint16_t port, uint32_t timeoutMs = 5000);

    ClientStatus Status() { return status; }

    void Poll(entt::dispatcher& dispatcher, float dt, uint32_t timeoutMs = 0);

    void Disconnect(uint32_t timeoutMs = 3000);

    void Shutdown();

    net::Buffer StartPacket(size_t size);

    void Send(net::Buffer data, SendType sendType, uint8_t channel = 0);

   private:
    void OnPacketReceived(uint8_t* data, size_t length)
    {
        LOG_INFO("Client received {} bytes", length);
    }

   private:
    float connectWaitTime;
    ClientStatus status = ClientStatus::Disconnected;
    ENetPeer* peer = nullptr;

    std::pmr::memory_resource* m_memory;
};
}  // namespace oge::runtime
