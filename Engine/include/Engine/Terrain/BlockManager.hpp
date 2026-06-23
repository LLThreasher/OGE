#pragma once
#include <cinttypes>

namespace OneGame::Engine::Terrain
{
    // support up to 4096 blocks, 256 of which are non-opaque
    constexpr uint32_t OPAQUE_BLOCK_START = 256;
    constexpr uint32_t BLOCK_ID_MASK = (uint32_t)(((uint64_t)1 << 12) - 1);
    constexpr uint32_t BLOCK_META_LIGHT_MASK = (uint32_t)(((uint64_t)1 << 4) - 1) << 12;
    constexpr uint32_t BLOCK_META_COLOR_MASK = (uint32_t)(((uint64_t)1 << 4) - 1) << 16;

    using BlockMetadata = uint16_t;

    inline bool IsOpaque(uint32_t blockValue)
    {
        return (blockValue & BLOCK_ID_MASK) >= OPAQUE_BLOCK_START;
    }

    inline uint8_t GetTextureSlot(uint32_t blockValue)
    {
        // auto id = (blockValue & BLOCK_ID_MASK);
        return 0;
    }

    inline BlockMetadata GetMetadata(uint32_t blockValue)
    {
        BlockMetadata result;
        result = GetTextureSlot(blockValue);
        result |= (blockValue & BLOCK_META_LIGHT_MASK) >> (12 - 8);
        result |= (blockValue & BLOCK_META_COLOR_MASK) >> (16 - 8);
        return result;
    }

    constexpr uint32_t BLOCK_FLAG_OPAQUE_TO_MESHER  = 1 << 0;
    constexpr uint32_t BLOCK_FLAG_OPAQUE_TO_LIGHT   = 1 << 1;

    struct BlockConfig
    {
        std::string blockDisplayName;
        std::array<uint8_t, 6> textureSlotPerFace;
        uint32_t blockFlags;
    };

    class BlockRegistry
    {
    public:
        void RegisterBlock(std::string blockIdName, BlockConfig config);
        uint32_t GetBlockId(std::string blockIdName);
    private:

    };
}
