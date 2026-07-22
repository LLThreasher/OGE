#include "oge/runtime/ring_staging_buffer.hpp"

#include "oge/graphics/backend.hpp"
#include "oge/math.hpp"

#define LOGGER_NAME "Engine"
#include "oge/log.hpp"

namespace oge::runtime
{
using namespace graphics;

    void RingStagingBuffer::Initialize(IGraphicsBackend& backend, uint64_t size)
    {
        using namespace oge::flag_helper;
        m_allocator.Initialize(size, 4);

        BufferDesc desc;
        desc.size = size;
        desc.memory = MemoryUsage::CPUToGPU;
        desc.usage = BufferUsage::TransferSrc | BufferUsage::TransferDst;

        m_buffer = backend.CreateBuffer(desc, &m_mappedPtr);
    }

    void RingStagingBuffer::Shutdown(IGraphicsBackend& backend)
    {
        backend.DestroyBuffer(m_buffer);
    }

    bool RingStagingBuffer::TryAllocate(uint64_t size, StagingAllocation& out)
    {
        RingAllocator::Allocation alloc;
        if (!m_allocator.TryAllocate(size, alloc)) return false;

        out.offset = alloc.offset;
        out.size = alloc.size;
        out.cpuPtr = static_cast<uint8_t*>(m_mappedPtr) + alloc.offset;
        return true;
    }

    void RingStagingBuffer::Free(uint64_t offset, uint64_t size)
    {
        m_allocator.Free(offset, size);
    }

    void RingStagingBuffer::Flush(IGraphicsBackend& backend)
    {
        uint32_t head = m_allocator.Head();
        uint32_t last = m_allocator.LastFlushHead();
        uint32_t cap = m_allocator.Capacity();

        GPUBufferSpan ranges[2];
        uint32_t count = 0;

        if (head > last)
        {
            ranges[count++] = {last, head - last, m_buffer};
        }
        else if (head < last)
        {
            ranges[count++] = {last, cap - last, m_buffer};
            ranges[count++] = {0, head, m_buffer};
        }

        if (count) backend.FlushStagingBufferRanges(std::span(ranges, count));

        m_allocator.MarkFlushed();
    }
}  // namespace OneGame::Engine::Graphics