#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <span>
#include <vector>
#include <unordered_map>
#include <cassert>

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
};

class CpuRingBuffer
{
public:
    explicit CpuRingBuffer(uint64_t size)
    {
        m_allocator.Initialize(size, 4);
        m_memory.resize(size);
    }

    bool TryAllocate(uint64_t size, std::span<std::byte>& out)
    {
        RingAllocator::Allocation alloc;
        if (m_allocator.TryAllocate(size, alloc))
        {
            out = std::span<std::byte>{m_memory.data() + alloc.offset, static_cast<size_t>(alloc.size)};
            return true;
        }
        return false;
    }

    void Free(std::span<std::byte> span)
    {
        assert(&span[0] - m_memory.data() < m_memory.size());
        m_allocator.Free(&span[0] - m_memory.data(), span.size());
    }

private:
    RingAllocator m_allocator;
    std::vector<std::byte> m_memory;
};

class CpuRingAllocator
{
public:
    explicit CpuRingAllocator(uint64_t size)
    {
        m_allocator.Initialize(size, alignof(std::max_align_t));
        m_memory.resize(size);
    }

    void* malloc(uint64_t size)
    {
        const uint64_t totalSize = size + sizeof(uint64_t);

        RingAllocator::Allocation alloc;
        if (!m_allocator.TryAllocate(totalSize, alloc))
            return nullptr;

        // write header
        auto* header = reinterpret_cast<uint64_t*>(&m_memory[alloc.offset]);
        *header = totalSize;

        return &m_memory[alloc.offset + sizeof(uint64_t)];
    }

    void Free(void* ptr)
    {
        assert(ptr != nullptr);

        auto* base = m_memory.data();
        auto* userPtr = reinterpret_cast<std::byte*>(ptr);

        uint64_t userOffset = userPtr - base;
        assert(userOffset >= sizeof(uint64_t));

        uint64_t headerOffset = userOffset - sizeof(uint64_t);

        auto* header = reinterpret_cast<uint64_t*>(&m_memory[headerOffset]);
        uint64_t totalSize = *header;

        m_allocator.Free(headerOffset, totalSize);
    }

private:
    RingAllocator m_allocator;
    std::vector<std::byte> m_memory;
};

}  // namespace oge