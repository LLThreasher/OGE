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

    RecordSend(data.Data().size());

    if (enet_peer_send(peer, channel, packet) < 0)
    {
        LOG_ERROR("failed on packet of size {} to peer {}", data.Data().size(),
                 peer->incomingPeerID);
        enet_packet_destroy(packet);
    }
}

void NetPacketSender::UpdatePacketStats(float dt)
{
    if (packetStatsLogInterval <= 0.0f)
        return;

    packetStatsLogTimer += dt;

    if (packetStatsLogTimer < packetStatsLogInterval)
        return;

    const float elapsed = packetStatsLogTimer;
    packetStatsLogTimer = 0.0f;

    const float sentBytesPerSecond =
        static_cast<float>(packetStats.intervalBytesSent) / elapsed;

    const float receivedBytesPerSecond =
        static_cast<float>(packetStats.intervalBytesReceived) / elapsed;

    const float sentPacketsPerSecond =
        static_cast<float>(packetStats.intervalPacketsSent) / elapsed;

    const float receivedPacketsPerSecond =
        static_cast<float>(packetStats.intervalPacketsReceived) / elapsed;

    LOG_INFO(
        "Net stats: sent={} bytes, recv={} bytes, sentPkts={}, recvPkts={}, "
        "sendRate={:.2f} B/s, recvRate={:.2f} B/s, sendPps={:.2f}, recvPps={:.2f}, "
        "totalSent={} bytes, totalRecv={} bytes",
        packetStats.intervalBytesSent,
        packetStats.intervalBytesReceived,
        packetStats.intervalPacketsSent,
        packetStats.intervalPacketsReceived,
        sentBytesPerSecond,
        receivedBytesPerSecond,
        sentPacketsPerSecond,
        receivedPacketsPerSecond,
        packetStats.bytesSent,
        packetStats.bytesReceived);

    packetStats.intervalBytesSent = 0;
    packetStats.intervalBytesReceived = 0;
    packetStats.intervalPacketsSent = 0;
    packetStats.intervalPacketsReceived = 0;
}
}  // namespace oge::runtime
