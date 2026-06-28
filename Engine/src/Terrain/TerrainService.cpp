#include "Engine/Terrain/BlockManager.hpp"
#include "Engine/Terrain/Terrain.hpp"

namespace OneGame::Engine::Terrain
{
void TerrainService::Initialize(const TerrainDesc& desc, Point3 chunkOrigin)
{
    m_terrainGenerator.SetTerrainGenChunkBudget(desc.terrainGenChunkBudget);
    m_terrainMeshBuilder.SetVertexBudget(desc.meshingQuadBudget);
    m_terrainUpdateScheduler.SetChunkViewDistance(desc.chunkViewDistance);
}

uint32_t TerrainService::GetBlock(int x, int y, int z)
{
    Point3 chunkCoord = {x >> 4, y >> 4, z >> 4};
    auto [_, chunk] = GetChunk(chunkCoord);
    assert(chunk != nullptr);
    if (chunk != nullptr) return chunk->GetBlock(x & 0xF, y & 0xF, z & 0xF);
    return 0;
}

void TerrainService::SetBlock(int x, int y, int z, uint32_t value)
{
    Point3 chunkCoord = {x >> 4, y >> 4, z >> 4};
    auto [_, chunk] = GetChunk(chunkCoord);
    assert(chunk != nullptr);
    if (chunk != nullptr) return chunk->SetBlock(x & 0xF, y & 0xF, z & 0xF, value);
}

std::tuple<ChunkHandle, ChunkData*> TerrainService::GetChunk(Point3 chunkCoord)
{
    return m_terrainData.chunks.Get(chunkCoord);
}

ChunkData* TerrainService::GetChunk(ChunkHandle handle) { return m_terrainData.chunks.Get(handle); }

void TerrainService::SubmitChunk(ChunkHandle handle) { m_terrainData.dirtyChunks.insert(handle); }

void TerrainService::Update(Point3 chunkOrigin, std::array<math::vec3, 6> frustum)
{
    for (auto chunk : m_terrainData.dirtyChunks)
    {
        m_terrainPData.buildMeshQueue.push(chunk);
    }
    m_terrainData.dirtyChunks.clear();
    m_terrainUpdateScheduler.UpdateChunkVisibility(m_terrainData, chunkOrigin, frustum);
}

void TerrainService::BuildTerrainMesh(BlockRegistry& blocks)
{
    m_terrainMeshBuilder.BuildChunkMeshes(m_terrainData, blocks, m_terrainPData);
}

void TerrainService::UploadBuiltChunks(StreamingManager& stream)
{
    m_terrainUploader.UploadTerrain(m_terrainPData, stream);
}
}  // namespace OneGame::Engine::Terrain
