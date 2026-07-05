#include "Engine/Graphics/RingStagingBuffer.hpp"

#include "Engine/Graphics/IGraphicsBackend.hpp"
#include "Engine/Math.hpp"

#define LOGGER_NAME "Engine"
#include "Engine/Logger.hpp"

namespace OneGame::Engine::Graphics
{
void RingStagingBuffer::Initialize(IGraphicsBackend& backend, uint64_t size)
{
    m_capacity = size;
    m_alignment = 4;

    // Create buffer
    {
        BufferDesc desc;
        desc.size = size;
        desc.memory = MemoryUsage::CPUToGPU;
        desc.usage = BufferUsage::TransferSrc | BufferUsage::TransferDst;
        m_buffer = backend.CreateBuffer(desc, &m_mappedPtr);
    }

    m_head = 0;
    m_tail = 0;
}

void RingStagingBuffer::Shutdown(IGraphicsBackend& backend) { backend.DestroyBuffer(m_buffer); }

bool RingStagingBuffer::TryAllocate(uint64_t size, StagingAllocation& out)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    out = {};
    size = math::align(size, m_alignment);

    // Check overlap
    if (m_head >= m_tail)
    {
        if (m_head + size > m_capacity && size > m_tail)
        {
            // LOG_DEBUG("TryAlloc failed: {}, {}, {}", m_pendingFrees.size(), m_head,
            // m_tail);
            return false;
        }
    }
    else
    {
        if (m_head + size >= m_tail)
        {
            // LOG_DEBUG("TryAlloc failed: {}, {}, {}", m_pendingFrees.size(), m_head,
            // m_tail);
            return false;
        }
    }

    // Wrap if needed
    if (m_head + size > m_capacity)
    {
        // add dummy free
        if (m_tail == m_head)
        {
            m_tail = 0;
        }
        else
        {
            m_pendingFrees[m_head] = m_capacity - m_head;
        }
        m_head = 0;
    }

    out.offset = m_head;
    out.size = size;
    out.cpuPtr = static_cast<uint8_t*>(m_mappedPtr) + m_head;

    m_head += size;

    return true;
}

StagingAllocation RingStagingBuffer::Allocate(uint64_t size)
{
    StagingAllocation alloc;
    if (!TryAllocate(size, alloc)) assert(false && "Staging buffer overflow");
    return alloc;
}

void RingStagingBuffer::Free(uint64_t offset, uint64_t size)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    // 1. Record this freed block
    m_pendingFrees[offset] = size;

    // 2. Advance the tail as far as we can using consecutive completed blocks
    auto it = m_pendingFrees.find(m_tail);
    while (it != m_pendingFrees.end())
    {
        uint64_t blockSize = it->second;

        // Remove from tracking and advance tail safely
        m_pendingFrees.erase(it);
        m_tail = (m_tail + blockSize) % m_capacity;

        // Look for the next contiguous block
        it = m_pendingFrees.find(m_tail);
    }
}

void RingStagingBuffer::Flush(IGraphicsBackend& backend)
{
    GPUBufferRange ranges[2] = {};
    if (m_head > m_lastFlushTail)
    {
        ranges[0].buffer = m_buffer;
        ranges[0].offset = m_lastFlushTail;
        ranges[0].size = m_head - m_lastFlushTail;
        backend.FlushStagingBufferRanges(std::span<GPUBufferRange>(ranges, 1));
    }
    else if (m_head < m_lastFlushTail)
    {
        ranges[0].buffer = m_buffer;
        ranges[0].offset = m_lastFlushTail;
        ranges[0].size = m_capacity - m_lastFlushTail;
        ranges[1].buffer = m_buffer;
        ranges[1].offset = 0;
        ranges[1].size = m_head;
        backend.FlushStagingBufferRanges(std::span<GPUBufferRange>(ranges, 2));
    }
    m_lastFlushTail = m_head;
}
}  // namespace OneGame::Engine::Graphics