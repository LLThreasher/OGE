#include "Engine/Graphics/RingStagingBuffer.hpp"
#include "Engine/Math.hpp"
#include "Engine/Graphics/IGraphicsBackend.hpp"

namespace OneGame::Engine::Graphics
{
    void RingStagingBuffer::Initialize(
        IGraphicsBackend& backend,
        uint64_t size)
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

    void RingStagingBuffer::Shutdown(IGraphicsBackend& backend)
    {
        backend.DestroyBuffer(m_buffer);
    }

    StagingAllocation RingStagingBuffer::Allocate(
        uint64_t size)
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        size = math::align(size, m_alignment);

        // Wrap if needed
        if (m_head + size > m_capacity)
        {
            m_head = 0;
        }

        // Check overlap
        if (m_head >= m_tail)
        {
            if (m_head + size > m_capacity &&
                size > m_tail)
            {
                assert(false && "Staging buffer overflow");
            }
        }
        else
        {
            if (m_head + size >= m_tail)
            {
                assert(false && "Staging buffer overflow");
            }
        }

        StagingAllocation alloc;
        alloc.offset = m_head;
        alloc.size = size;
        alloc.cpuPtr = static_cast<uint8_t*>(m_mappedPtr) + m_head;

        m_head += size;

        return alloc;
    }

    void RingStagingBuffer::Free(uint64_t offset, uint64_t size)
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        // Only advance tail if freeing oldest block
        if (offset == m_tail)
        {
            m_tail = offset + size;

            if (m_tail >= m_capacity)
                m_tail = 0;
        }
    }

}