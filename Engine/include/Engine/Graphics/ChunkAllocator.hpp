#pragma once
#include <array>
#include <cassert>
#include <cstdint>
#include <vector>

namespace OneGame::Engine::Graphics
{
class ChunkAllocator
{
   public:
    // maxChunks must be divisible by 4
    explicit ChunkAllocator() {}

    void Initialize(uint32_t maxChunks)
    {
        assert(maxChunks % 4 == 0);

        m_blockOrder.resize(maxChunks);
        // Initially everything is a size-4 block (order 2)
        for (uint32_t i = 0; i < maxChunks; i += 4)
        {
            m_freeLists[2].push_back(i);
            m_blockOrder[i] = 2;
        }
    }

    // size must be 1, 2, or 4
    int Allocate(uint32_t size)
    {
        uint32_t order = SizeToOrder(size);
        return AllocateOrder(order);
    }

    void Free(uint32_t index, uint32_t size)
    {
        uint32_t order = SizeToOrder(size);
        FreeOrder(index, order);
    }

    uint32_t GetMaxNumChunks() const
    {
        return m_totalChunks;
    }

   private:
    static constexpr uint32_t INVALID_ORDER = 0xFFFFFFFF;

    uint32_t m_totalChunks;

    // Free lists for order 0, 1, 2
    std::array<std::vector<uint32_t>, 3> m_freeLists;

    // Tracks the order of each block (only valid at block starts)
    std::vector<uint32_t> m_blockOrder;

    static uint32_t SizeToOrder(uint32_t size)
    {
        switch (size)
        {
            case 1:
                return 0;
            case 2:
                return 1;
            case 4:
                return 2;
            default:
                assert(false && "Invalid allocation size");
                return 0;
        }
    }

    static uint32_t OrderToSize(uint32_t order) { return 1u << order; }

    uint32_t BuddyIndex(uint32_t index, uint32_t order) const { return index ^ OrderToSize(order); }

    int AllocateOrder(uint32_t order)
    {
        // Try exact order
        if (!m_freeLists[order].empty())
        {
            uint32_t index = m_freeLists[order].back();
            m_freeLists[order].pop_back();

            m_blockOrder[index] = INVALID_ORDER;  // now allocated
            return static_cast<int>(index);
        }

        // Try higher orders and split
        for (uint32_t higher = order + 1; higher <= 2; ++higher)
        {
            if (!m_freeLists[higher].empty())
            {
                uint32_t index = m_freeLists[higher].back();
                m_freeLists[higher].pop_back();

                // Split downward
                while (higher > order)
                {
                    --higher;
                    uint32_t buddy = index + OrderToSize(higher);

                    m_freeLists[higher].push_back(buddy);
                    m_blockOrder[buddy] = higher;
                }

                m_blockOrder[index] = INVALID_ORDER;
                return static_cast<int>(index);
            }
        }

        return -1;  // out of memory
    }

    void FreeOrder(uint32_t index, uint32_t order)
    {
        assert(index < m_totalChunks);

        while (order < 2)
        {
            uint32_t buddy = BuddyIndex(index, order);

            if (buddy >= m_totalChunks) break;

            if (m_blockOrder[buddy] != order) break;  // buddy not free or different size

            // Remove buddy from free list
            RemoveFromFreeList(buddy, order);

            // Merge
            index = std::min(index, buddy);
            ++order;
        }

        m_blockOrder[index] = order;
        m_freeLists[order].push_back(index);
    }

    void RemoveFromFreeList(uint32_t index, uint32_t order)
    {
        auto& list = m_freeLists[order];

        for (size_t i = 0; i < list.size(); ++i)
        {
            if (list[i] == index)
            {
                list[i] = list.back();
                list.pop_back();
                return;
            }
        }

        assert(false && "Buddy not found in free list");
    }
};
}  // namespace OneGame::Engine::Graphics