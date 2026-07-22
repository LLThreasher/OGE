#pragma once

#include <cstdlib>
#include <vector>
#include <unordered_map>
#include <mutex>

namespace oge
{
class RingAllocator
{
   public:
    struct Allocation
    {
        uint64_t offset = 0;
        uint64_t size = 0;
    };
    
    void Initialize(uint64_t capacity, uint64_t alignment = 4);

    bool TryAllocate(uint64_t size, Allocation& out);

    void Free(uint64_t offset, uint64_t size);

    uint64_t Head() const { return m_head; }
    uint64_t Tail() const { return m_tail; }
    uint64_t Capacity() const { return m_capacity; }

    uint64_t LastFlushHead() const { return m_lastFlushHead; }
    void MarkFlushed() { m_lastFlushHead = m_head; }

   private:
    uint64_t Align(uint64_t v) const;

    bool HasSpace(uint64_t size) const;

   private:
    uint64_t m_capacity = 0;
    uint64_t m_alignment = 4;

    uint64_t m_head = 0;
    uint64_t m_tail = 0;
    uint64_t m_lastFlushHead = 0;

    std::unordered_map<uint64_t, uint64_t> m_pendingFrees;
    mutable std::mutex m_mutex;
};

class CpuRingBuffer
{
public:
    void Initialize(uint64_t size)
    {
        m_allocator.Initialize(size, 4);
        m_memory.resize(size);
    }

    bool TryAllocate(uint64_t size, RingAllocator::Allocation& out)
    {
        return m_allocator.TryAllocate(size, out);
    }

    void* GetPointer(uint64_t offset)
    {
        return m_memory.data() + offset;
    }

    void Free(uint64_t offset, uint64_t size)
    {
        m_allocator.Free(offset, size);
    }

private:
    RingAllocator m_allocator;
    std::vector<uint8_t> m_memory;
};

}  // namespace oge