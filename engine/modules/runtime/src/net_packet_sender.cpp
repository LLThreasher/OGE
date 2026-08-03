#include "oge/runtime/net_packet_sender.hpp"

#include <enet/enet.h>
#include "enet_interface.hpp"
#include "oge/log.hpp"

namespace oge::runtime
{

net::Buffer NetPacketSender::StartPacket(size_t size)
{
    return net::Buffer(oge_enet_memory->allocate(size), size);
}

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
        LOG_ERROR("failed on packet of size {} to peer {}", data.Data().size(),
                 peer->incomingPeerID);
        enet_packet_destroy(packet);
    }
    else
    {
        LOG_INFO("sent packet of size {} to peer {}", data.Data().size(),
                 peer->incomingPeerID);
    }
}
}  // namespace oge::runtime
