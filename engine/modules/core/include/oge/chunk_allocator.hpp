#pragma once
#include <array>
#include <cassert>
#include <cstdint>
#include <vector>
#include "oge/macros.hpp"

namespace oge
{
class ChunkAllocator
{
   public:
    // maxChunks must be divisible by 4
    explicit ChunkAllocator(uint32_t maxChunks);

    // size must be 1, 2, or 4
    int Allocate(uint32_t size);

    void Free(uint32_t index, uint32_t size);

    uint32_t GetMaxNumChunks() const;

   private:
    static constexpr uint32_t INVALID_ORDER = 0xFFFFFFFF;

    uint32_t m_totalChunks;

    // Free lists for order 0, 1, 2
    std::array<std::vector<uint32_t>, 3> m_freeLists;

    // Tracks the order of each block (only valid at block starts)
    std::vector<uint32_t> m_blockOrder;

    static uint32_t SizeToOrder(uint32_t size);

    static uint32_t OrderToSize(uint32_t order);

    uint32_t BuddyIndex(uint32_t index, uint32_t order) const;

    int AllocateOrder(uint32_t order);

    void FreeOrder(uint32_t index, uint32_t order);

    void RemoveFromFreeList(uint32_t index, uint32_t order);
};
}  // namespace OneGame::Engine::Graphics