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

        // Case 1: head >= tail
        if (m_head >= m_tail)
        {
            // Try allocate without wrap
            if (m_head + size <= m_capacity)
            {
                out.offset = m_head;
                out.size = size;
                m_head += size;
                return true;
            }

            // Try wrap to zero
            if (size < m_tail)
            {
                // Mark remaining region as padding
                if (m_head != m_capacity)
                    m_pendingFrees[m_head] = m_capacity - m_head;

                m_head = 0;

                out.offset = m_head;
                out.size = size;
                m_head += size;
                return true;
            }

            return false;
        }
        else
        {
            // Case 2: head < tail
            if (m_head + size < m_tail)
            {
                out.offset = m_head;
                out.size = size;
                m_head += size;
                return true;
            }

            return false;
        }
    }
    
    void RingAllocator::Free(uint64_t offset, uint64_t size)
    {
        size = Align(size);

        m_pendingFrees[offset] = size;

        while (true)
        {
            auto it = m_pendingFrees.find(m_tail);
            if (it == m_pendingFrees.end())
                break;

            uint64_t blockSize = it->second;
            m_pendingFrees.erase(it);

            m_tail = (m_tail + blockSize) % m_capacity;
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