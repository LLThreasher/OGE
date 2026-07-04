#pragma once
#include <cassert>
#include <cstdint>
#include <map>
#include <mutex>

#include "Engine/ObjectType.hpp"

namespace OneGame::Engine::Graphics
{
class IGraphicsBackend;

class RingStagingBuffer
{
   public:
    void Initialize(IGraphicsBackend& backend, uint64_t size);
    void Shutdown(IGraphicsBackend& backend);

    StagingAllocation Allocate(uint64_t size);
    bool TryAllocate(uint64_t size, StagingAllocation& out);
    void Free(uint64_t offset, uint64_t size);

    GPUBufferHandle GetBuffer() const { return m_buffer; }

    void Flush(IGraphicsBackend& backend);

   private:
    GPUBufferHandle m_buffer;
    std::map<uint64_t, uint64_t> m_pendingFrees;  // Maps offset -> size

    void* m_mappedPtr = nullptr;

    uint64_t m_capacity = 0;
    uint64_t m_head = 0;
    uint64_t m_tail = 0;
    uint64_t m_alignment;

    std::mutex m_mutex;

    uint64_t m_lastFlushTail = 0;
};
}  // namespace OneGame::Engine::Graphics