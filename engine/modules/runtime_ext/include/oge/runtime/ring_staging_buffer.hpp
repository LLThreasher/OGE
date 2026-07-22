#pragma once
#include <cassert>
#include <cstdint>

#include "oge/graphics/objects.hpp"
#include "oge/runtime/objects_ext.hpp"
#include "oge/ring_allocator.hpp"

namespace oge::graphics
{
class IGraphicsBackend;
}

namespace oge::runtime
{
using ::oge::graphics::IGraphicsBackend;

class RingStagingBuffer
{
   public:
    void Initialize(IGraphicsBackend& backend, uint64_t size);

    void Shutdown(IGraphicsBackend& backend);

    bool TryAllocate(uint64_t size, StagingAllocation& out);

    void Free(uint64_t offset, uint64_t size);

    void Flush(IGraphicsBackend& backend);

    GPUBufferHandle GetBuffer() const { return m_buffer; }

   private:
    RingAllocator m_allocator;

    GPUBufferHandle m_buffer{};
    void* m_mappedPtr = nullptr;
};
}  // namespace oge::runtime
