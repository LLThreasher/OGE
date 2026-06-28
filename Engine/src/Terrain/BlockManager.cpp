#include "Engine/Terrain/BlockManager.hpp"

namespace OneGame::Engine::Terrain
{
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

uint16_t BlockRegistry::GetBlockId(std::string blockIdName) { return m_idNameToBlockId[blockIdName]; }

uint16_t BlockRegistry::GetBlockId(uint32_t blockValue) { return blockValue & BLOCK_ID_MASK; }

bool BlockRegistry::IsOpaque(uint16_t blockIdx) { return m_blockFlags[blockIdx] & BLOCK_FLAG_OPAQUE_TO_MESHER; }

std::array<uint8_t, 6>& BlockRegistry::GetTextureSlot(uint16_t blockIdx) { return m_textureSlots[blockIdx]; }
}  // namespace OneGame::Engine::Terrain
