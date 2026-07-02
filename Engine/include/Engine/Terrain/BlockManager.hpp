#pragma once
#include <array>
#include <cinttypes>
#include <string>
#include <unordered_map>
#include <vector>

namespace OneGame::Engine::Terrain
{
// support up to 4096 blocks, 256 of which are non-opaque
constexpr uint32_t OPAQUE_BLOCK_START = 256;
constexpr uint32_t BLOCK_ID_MASK = (uint32_t)(((uint64_t)1 << 12) - 1);
constexpr uint32_t BLOCK_META_LIGHT_MASK = (uint32_t)(((uint64_t)1 << 4) - 1) << 12;
constexpr uint32_t BLOCK_META_COLOR_MASK = (uint32_t)(((uint64_t)1 << 4) - 1) << 16;

using BlockMetadata = uint8_t;

inline BlockMetadata GetMetadata(uint32_t blockValue)
{
    BlockMetadata result;
    result = (blockValue & (BLOCK_META_LIGHT_MASK | BLOCK_META_COLOR_MASK)) >> 12;
    return result;
}

constexpr uint32_t BLOCK_FLAG_OPAQUE_TO_MESHER = 1 << 0;
constexpr uint32_t BLOCK_FLAG_OPAQUE_TO_LIGHT = 1 << 1;

struct BlockConfig
{
    std::string blockDisplayName;
    std::array<std::string, 6> textureSlotPerFace;
    uint32_t blockFlags;

    BlockConfig(std::string blockDisplayName = "Air", std::string textureId = "invalid.png", uint32_t blockFlags = 0) :
        blockDisplayName(blockDisplayName), blockFlags(blockFlags)
    {
        for (int i = 0; i < 6; ++i)
        {
            textureSlotPerFace[i] = textureId;
        }
    }
};

class BlockRegistry
{
   public:
    BlockRegistry();
    void RegisterBlock(std::string blockIdName, BlockConfig config);
    const std::vector<std::string>& GetBlockTextureArray()
    {
        return m_blockTextureArray;
    }
    static uint16_t GetBlockId(uint32_t blockValue);
    uint16_t GetBlockId(const std::string blockIdName) const;
    const std::string& GetBlockDisplayName(uint16_t blockIdx) const;
    bool IsOpaque(uint16_t blockIdx) const;
    const std::array<uint8_t, 6>& GetTextureSlot(uint16_t blockIdx) const;

   private:
    std::unordered_map<std::string, uint32_t> m_blockTextureIds;
    std::vector<std::string> m_blockTextureArray;
    std::unordered_map<std::string, uint16_t> m_idNameToBlockId;
    std::vector<std::string> m_blockDisplayNames;
    std::vector<uint32_t> m_blockFlags;
    std::vector<std::array<uint8_t, 6>> m_textureSlots;
    uint32_t m_nextIdx = 0;
};
}  // namespace OneGame::Engine::Terrain
