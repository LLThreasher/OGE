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
    struct PacketStats
    {
        uint64_t bytesSent = 0;
        uint64_t bytesReceived = 0;
        uint64_t packetsSent = 0;
        uint64_t packetsReceived = 0;

        uint64_t intervalBytesSent = 0;
        uint64_t intervalBytesReceived = 0;
        uint64_t intervalPacketsSent = 0;
        uint64_t intervalPacketsReceived = 0;
    };

   public:
    net::Buffer StartPacket(size_t size);

    void Send(
        ENetPeer* peer,
        net::Buffer data,
        SendType sendType = SendType::Reliable,
        uint8_t channel = 0);

    void UpdatePacketStats(float dt);

    void SetPacketStatsLogInterval(float seconds)
    {
        packetStatsLogInterval = seconds;
    }

    const PacketStats& GetPacketStats() const
    {
        return packetStats;
    }

   protected:
    void RecordReceive(size_t bytes)
    {
        packetStats.bytesReceived += bytes;
        packetStats.packetsReceived += 1;

        packetStats.intervalBytesReceived += bytes;
        packetStats.intervalPacketsReceived += 1;
    }

    void RecordSend(size_t bytes)
    {
        packetStats.bytesSent += bytes;
        packetStats.packetsSent += 1;

        packetStats.intervalBytesSent += bytes;
        packetStats.intervalPacketsSent += 1;
    }

   protected:
    ENetHost* host = nullptr;

   private:
    PacketStats packetStats = {};

    float packetStatsLogTimer = 0.0f;
    float packetStatsLogInterval = 1.0f;
};
}  // namespace oge::runtime
