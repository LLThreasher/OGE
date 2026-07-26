#pragma once

#include "oge/runtime/net_serializer.hpp"

struct _ENetPeer;
struct _ENetHost;
typedef _ENetPeer ENetPeer;
typedef _ENetHost ENetHost;
namespace oge::runtime
{
enum class SendType
{
    Reliable,
    Sequenced,
    Unreliable,
};

class NetPacketSender
{
   public:
    void Send(ENetPeer* peer, net::Buffer data,
              SendType sendType = SendType::Reliable, uint8_t channel = 0);

   protected:
    ENetHost* host = nullptr;
};
}  // namespace oge::runtime
