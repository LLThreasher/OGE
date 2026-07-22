#include "oge/ring_allocator.hpp"


namespace oge
{

    void RingAllocator::Initialize(uint64_t capacity, uint64_t alignment)
    {
        m_capacity = capacity;
        m_alignment = alignment;
        m_head = 0;
        m_tail = 0;
        m_lastFlushHead = 0;
    }

    bool RingAllocator::TryAllocate(uint64_t size, Allocation& out)
    {
        size = Align(size);

        if (!HasSpace(size)) return false;

        if (m_head + size > m_capacity)
        {
            // wrap
            if (m_tail != m_head) m_pendingFrees[m_head] = m_capacity - m_head;

            m_head = 0;
        }

        out.offset = m_head;
        out.size = size;

        m_head += size;
        return true;
    }

    void RingAllocator::Free(uint64_t offset, uint64_t size)
    {
        m_pendingFrees[offset] = size;

        auto it = m_pendingFrees.find(m_tail);
        while (it != m_pendingFrees.end())
        {
            uint64_t blockSize = it->second;
            m_pendingFrees.erase(it);
            m_tail = (m_tail + blockSize) % m_capacity;
            it = m_pendingFrees.find(m_tail);
        }
    }

    uint64_t RingAllocator::Align(uint64_t v) const
    {
        return (v + m_alignment - 1) & ~(m_alignment - 1);
    }

    bool RingAllocator::HasSpace(uint64_t size) const
    {
        if (m_head >= m_tail)
        {
            if (m_head + size > m_capacity && size > m_tail) return false;
        }
        else
        {
            if (m_head + size >= m_tail) return false;
        }
        return true;
    }
}