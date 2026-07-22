#pragma once

#include "oge/graphics/backend.hpp"
#include "oge/graphics/objects.hpp"
#include "oge/runtime/objects_ext.hpp"
#include "oge/chunk_allocator.hpp"

namespace oge::runtime::gfx
{
using namespace oge::graphics;

namespace dca
{
constexpr uint32_t _1K = 1024;
constexpr uint32_t _1M = 1024 * _1K;
constexpr uint32_t CHUNK_SIZE_COUNT = 6;
// max 128 KB per chunk
constexpr uint32_t CHUNK_SIZES[] = {
    4 * _1K, 8 * _1K, 16 * _1K, 32 * _1K, 64 * _1K, 128 * _1K,
};
// max 127 MB per chunk size -> 625 MB maximum
constexpr uint32_t BLOCK_SIZES[] = {
    2 * _1M, 4 * _1M, 8 * _1M, 16 * _1M, 32 * _1M, 64 * _1M,
};

class DynamicChunkAllocator
{
   public:
    void Clear(IGraphicsBackend& backend)
    {
        for (uint32_t i = 0; i < CHUNK_SIZE_COUNT; ++i)
        {
            for (auto handle : activeMemBlocks[i])
            {
                backend.DestroyBuffer(handle);
            }
            activeMemBlocks[i].clear();
            allocatorPerChunk[i].clear();
        }
    }

    GPUChunkedAllocation Allocate(IGraphicsBackend& backend, uint32_t size)
    {
        for (uint16_t i = 0; i < CHUNK_SIZE_COUNT; ++i)
        {
            if (size < CHUNK_SIZES[i])
            {
                if (allocatorPerChunk[i].empty())
                {
                    allocatorPerChunk[i].emplace_back(BLOCK_SIZES[allocatorPerChunk[i].size()] / CHUNK_SIZES[i]);
                    activeMemBlocks[i].push_back(
                        backend.AllocateGPUBuffer<BufferUsage::Storage>(BLOCK_SIZES[activeMemBlocks[i].size()]));
                }

                int allocatedSlot = allocatorPerChunk[i].back().Allocate(1);
                if (allocatedSlot == -1)
                {
                    allocatorPerChunk[i].emplace_back(BLOCK_SIZES[allocatorPerChunk[i].size()] / CHUNK_SIZES[i]);
                    allocatedSlot = allocatorPerChunk[i].back().Allocate(1);
                    activeMemBlocks[i].push_back(
                        backend.AllocateGPUBuffer<BufferUsage::Storage>(BLOCK_SIZES[activeMemBlocks[i].size()]));
                }
                uint16_t blockSlot = allocatorPerChunk[i].size() - 1;
                return {
                    i,
                    blockSlot,
                    (uint32_t)allocatedSlot,
                };
            }
        }
        assert(false && "allocation failure");
        return {0xffff};
    }

    void Free(GPUChunkedAllocation alloc)
    {
        assert(alloc.chunkSizeIdx < CHUNK_SIZE_COUNT);
        assert(alloc.blockIdx < allocatorPerChunk[alloc.chunkSizeIdx].size());
        allocatorPerChunk[alloc.chunkSizeIdx][alloc.blockIdx].Free(alloc.slotOffset, 1);
    }

    GPUBufferSpan Resolve(GPUChunkedAllocation alloc)
    {
        assert(alloc.chunkSizeIdx < CHUNK_SIZE_COUNT);
        return {
            alloc.slotOffset * CHUNK_SIZES[alloc.chunkSizeIdx],
            CHUNK_SIZES[alloc.chunkSizeIdx],
            activeMemBlocks[alloc.chunkSizeIdx][alloc.blockIdx],
        };
    }

   private:
    std::array<std::vector<GPUBufferHandle>, CHUNK_SIZE_COUNT> activeMemBlocks;
    std::array<std::vector<ChunkAllocator>, CHUNK_SIZE_COUNT> allocatorPerChunk;
};
}  // namespace dca
using DynamicChunkAllocator = dca::DynamicChunkAllocator;
}  // namespace oge::runtime::renderer