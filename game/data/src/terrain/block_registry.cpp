#include "game/terrain/block_registry.hpp"

#include <algorithm>
#include <array>
#include <span>
#include <vector>

#include "oge/aabb.hpp"

namespace game::terrain
{

BlockConfig::BlockConfig(std::string blockDisplayName, std::string textureId, uint32_t blockFlags,
                         std::vector<AABB> aabb)
    : blockDisplayName(blockDisplayName), blockFlags(blockFlags), aabbs(std::move(aabb))
{
    for (int i = 0; i < 6; ++i)
    {
        textureSlotPerFace[i] = textureId;
    }
}

BlockRegistry::BlockRegistry()
{
    BlockConfig airConfig = {};
    airConfig.aabbs = {};
    RegisterBlock("air", airConfig);
}

void BlockRegistry::RegisterBlock(std::string blockIdName, BlockConfig config)
{
    m_blockDisplayNames.resize(m_nextIdx + 1);
    m_blockFlags.resize(m_nextIdx + 1);
    m_textureSlots.resize(m_nextIdx + 1);
    m_aabbLookup.resize(m_nextIdx + 1);

    m_idNameToBlockId[blockIdName] = m_nextIdx;
    m_blockDisplayNames[m_nextIdx] = config.blockDisplayName;
    m_blockFlags[m_nextIdx] = config.blockFlags;
    for (int i = 0; i < 6; ++i)
    {
        auto& id = config.textureSlotPerFace[i];
        auto it = m_blockTextureIds.find(id);
        if (it == m_blockTextureIds.end())
        {
            auto [newIt, succ] = m_blockTextureIds.insert_or_assign(id, m_blockTextureArray.size());
            it = newIt;
            m_blockTextureArray.push_back(id);
        }
        m_textureSlots[m_nextIdx][i] = it->second;
    }
    auto it = std::search(m_aabbs.begin(), m_aabbs.end(), config.aabbs.begin(), config.aabbs.end());

    if (it != m_aabbs.end())
    {
        m_aabbLookup[m_nextIdx] = std::span<AABB>(it, config.aabbs.size());
    }
    else if (config.aabbs.empty())
    {
        m_aabbLookup[m_nextIdx] = std::span<AABB>();
    }
    else
    {
        for (const auto& aabb : config.aabbs)
        {
            m_aabbs.push_back(aabb);
        }

        m_aabbLookup[m_nextIdx] = std::span<AABB>(m_aabbs.end() - config.aabbs.size(), config.aabbs.size());
    }

    m_nextIdx += 1;
}

uint16_t BlockRegistry::GetBlockId(const std::string blockIdName) const
{
    return m_idNameToBlockId.find(blockIdName)->second;
}

uint16_t BlockRegistry::GetBlockId(uint32_t blockValue) { return blockValue & BLOCK_ID_MASK; }

const std::string& BlockRegistry::GetBlockDisplayName(uint16_t blockIdx) const { return m_blockDisplayNames[blockIdx]; }

bool BlockRegistry::IsOpaque(const uint16_t blockIdx) const
{
    return m_blockFlags[blockIdx] & BLOCK_FLAG_OPAQUE_TO_MESHER;
}

const std::array<uint8_t, 6>& BlockRegistry::GetTextureSlot(const uint16_t blockIdx) const
{
    return m_textureSlots[blockIdx];
}

AABBList BlockRegistry::GetBlockAABBList(uint16_t blockIdx) const { return m_aabbLookup[blockIdx]; }

AABBList BlockRegistry::GetDefaultBlockAABBList() { return std::span<const AABB>(&DEFAULT_BLOCK_AABB, 1); }

float BlockRegistry::GetBlockFriction(uint16_t blockIdx) const
{
    if (blockIdx == 0) return 0.01f;
    return 0.5f;
}

}  // namespace game::terrain
