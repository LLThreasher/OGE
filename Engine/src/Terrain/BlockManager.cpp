#include "Engine/Terrain/BlockManager.hpp"

namespace OneGame::Engine::Terrain
{
BlockRegistry::BlockRegistry()
{
    RegisterBlock("air", {"Air", {}, 0});
}

void BlockRegistry::RegisterBlock(std::string blockIdName, BlockConfig config)
{
    m_blockDisplayNames.resize(m_nextIdx + 1);
    m_blockFlags.resize(m_nextIdx + 1);
    m_textureSlots.resize(m_nextIdx + 1);

    m_idNameToBlockId[blockIdName] = m_nextIdx;
    m_blockDisplayNames[m_nextIdx] = config.blockDisplayName;
    m_blockFlags[m_nextIdx] = config.blockFlags;
    m_textureSlots[m_nextIdx] = config.textureSlotPerFace;

    m_nextIdx += 1;
}

uint16_t BlockRegistry::GetBlockId(const std::string blockIdName) const { return m_idNameToBlockId.find(blockIdName)->second; }

uint16_t BlockRegistry::GetBlockId(uint32_t blockValue) { return blockValue & BLOCK_ID_MASK; }

bool BlockRegistry::IsOpaque(const uint16_t blockIdx) const {
    return m_blockFlags[blockIdx] & BLOCK_FLAG_OPAQUE_TO_MESHER;
}

const std::array<uint8_t, 6>& BlockRegistry::GetTextureSlot(const uint16_t blockIdx) const
{
    return m_textureSlots[blockIdx];
}
}  // namespace OneGame::Engine::Terrain
