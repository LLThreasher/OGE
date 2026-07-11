#include "Engine/Terrain/BlockManager.hpp"

namespace OneGame::Engine::Terrain
{
BlockRegistry::BlockRegistry() { RegisterBlock("air", {}); }

void BlockRegistry::RegisterBlock(std::string blockIdName, BlockConfig config)
{
    m_blockDisplayNames.resize(m_nextIdx + 1);
    m_blockFlags.resize(m_nextIdx + 1);
    m_textureSlots.resize(m_nextIdx + 1);

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

const AABBList BlockRegistry::GetBlockAABBList(uint16_t blockIdx) const
{
    if (blockIdx == 0) return {};
    return {AABB{{0.f, 0.f, 0.f}, {1.f, 1.f, 1.f}}};
}

float BlockRegistry::GetBlockDrag(uint16_t blockIdx) const
{
    if (blockIdx == 0)
        return 0.001f;
    return 5.f;
}

}  // namespace OneGame::Engine::Terrain
