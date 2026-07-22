#pragma once
#include <cstddef>
#include <cstdint>
#include <span>

#include "oge/ring_allocator.hpp"
#include "oge/runtime/net_serializer.hpp"

namespace oge::runtime
{
class NetPacketProducer
{
   public:
    NetPacketProducer(uint64_t size) : packetSendBuffer(size) {}

   public:
    net::Buffer AllocPacket(uint64_t size)
    {
        std::span<std::byte> alloc;
        if (packetSendBuffer.TryAllocate(size, alloc))
            return net::Buffer(alloc);
        return net::Buffer(std::span<std::byte>{});
    }

    void FreePacket(net::Buffer buffer)
    {
        packetSendBuffer.Free(buffer.RawData());
    }

   private:
    CpuRingBuffer packetSendBuffer;
};
}  // namespace oge::runtime
