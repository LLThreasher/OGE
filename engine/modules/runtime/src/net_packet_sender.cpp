#include "oge/runtime/net_packet_sender.hpp"

#include <enet/enet.h>

namespace oge::runtime
{
void NetPacketSender::Send(ENetPeer* peer, net::Buffer data, SendType sendType,
                           uint8_t channel)
{
    int flag = 0;
    switch (sendType)
    {
        case SendType::Reliable:
            flag |= ENET_PACKET_FLAG_RELIABLE;
            break;
        case SendType::Sequenced:
            break;
        case SendType::Unreliable:
            flag |= ENET_PACKET_FLAG_NO_ALLOCATE;
            break;
    }

    auto packet =
        enet_packet_create(data.Data().data(), data.Data().size(), flag);
    if (enet_peer_send(peer, channel, packet) < 0)
    {
        enet_packet_destroy(packet);
    }
}
}  // namespace oge::runtime
