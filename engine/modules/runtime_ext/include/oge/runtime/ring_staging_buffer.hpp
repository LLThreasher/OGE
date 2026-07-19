#pragma once
#include <cassert>
#include <cstdint>
#include <map>
#include <mutex>

#include "oge/graphics/objects.hpp"
#include "oge/runtime/objects_ext.hpp"

namespace oge::graphics
{
class IGraphicsBackend;
}

namespace oge::runtime
{
class RingStagingBuffer
{
   public:
    void Initialize(graphics::IGraphicsBackend& backend, uint64_t size);
    void Shutdown(graphics::IGraphicsBackend& backend);

    StagingAllocation Allocate(uint64_t size);
    bool TryAllocate(uint64_t size, StagingAllocation& out);
    void Free(uint64_t offset, uint64_t size);

    GPUBufferHandle GetBuffer() const { return m_buffer; }

    void Flush(graphics::IGraphicsBackend& backend);

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