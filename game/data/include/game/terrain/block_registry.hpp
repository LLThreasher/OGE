#pragma once
#include <array>
#include <cinttypes>
#include <span>
#include <string>
#include <tuple>
#include <unordered_map>
#include <vector>

#include "oge/aabb.hpp"
#include "oge/fixed_vector.hpp"
#include "game/json.hpp"

namespace game::terrain
{
using oge::AABB;
// support up to 4096 blocks, 256 of which are non-opaque
constexpr uint32_t OPAQUE_BLOCK_START = 256;
constexpr uint32_t BLOCK_ID_MASK = (uint32_t)(((uint64_t)1 << 12) - 1);
constexpr uint32_t BLOCK_META_LIGHT_MASK = (uint32_t)(((uint64_t)1 << 4) - 1)
                                           << 12;
constexpr uint32_t BLOCK_META_COLOR_MASK = (uint32_t)(((uint64_t)1 << 4) - 1)
                                           << 16;

using BlockMetadata = uint8_t;

inline BlockMetadata GetMetadata(uint32_t blockValue)
{
    BlockMetadata result;
    result =
        (blockValue & (BLOCK_META_LIGHT_MASK | BLOCK_META_COLOR_MASK)) >> 12;
    return result;
}

constexpr uint32_t BLOCK_FLAG_OPAQUE_TO_MESHER = 1 << 0;
constexpr uint32_t BLOCK_FLAG_OPAQUE_TO_LIGHT = 1 << 1;

constexpr AABB DEFAULT_BLOCK_AABB = AABB{{0.f, 0.f, 0.f}, {1.f, 1.f, 1.f}};

struct BlockConfig
{
    std::string blockDisplayName;
    std::array<std::string, 6> textureSlotPerFace;
    uint32_t blockFlags;
    std::vector<AABB> aabbs;

    BlockConfig(std::string blockDisplayName = "Air",
                std::string textureId = "invalid.png", uint32_t blockFlags = 0,
                std::vector<AABB> aabbs = {DEFAULT_BLOCK_AABB});
};

// size 12 * 2 * 16 = 384 bytes
using AABBList = std::span<const AABB>;

class BlockRegistry
{
   public:
    BlockRegistry();
    void RegisterBlock(std::string blockIdName, BlockConfig config);

    static uint16_t GetBlockId(uint32_t blockValue);
    static AABBList GetDefaultBlockAABBList();

    const std::vector<std::string>& GetBlockTextures() const;
    uint16_t GetBlockId(const std::string blockIdName) const;
    const std::string& GetBlockDisplayName(uint16_t blockIdx) const;
    bool IsOpaque(uint16_t blockIdx) const;
    const std::array<uint8_t, 6>& GetTextureSlot(uint16_t blockIdx) const;
    AABBList GetBlockAABBList(uint16_t blockIdx) const;
    float GetBlockFriction(uint16_t blockIdx) const;

   private:
    std::unordered_map<std::string, uint32_t> m_blockTextureIds;
    std::vector<std::string> m_blockTextures;
    std::unordered_map<std::string, uint16_t> m_idNameToBlockId;
    std::vector<std::string> m_blockDisplayNames;
    std::vector<uint32_t> m_blockFlags;
    std::vector<std::array<uint8_t, 6>> m_textureSlots;
    uint32_t m_nextIdx = 0;

    std::vector<AABB> m_aabbs;
    std::vector<AABBList> m_aabbLookup;
};
}  // namespace game::terrain

DECL_JSON_OBJ(::oge::AABB, {
    visit("min.x", self.min.x);
    visit("min.y", self.min.y);
    visit("min.z", self.min.z);
    visit("max.x", self.max.x);
    visit("max.y", self.max.y);
    visit("max.z", self.max.z);
})

DECL_JSON_OBJ(::game::terrain::BlockConfig, {
    visit("display_name", self.blockDisplayName);
    visit("texture_slot_per_face", self.textureSlotPerFace);
    visit("block_flags", self.blockFlags);
    visit("aabbs", self.aabbs);
})
