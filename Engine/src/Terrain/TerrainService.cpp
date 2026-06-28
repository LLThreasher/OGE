#include "Engine/Terrain/BlockManager.hpp"
#include "Engine/Terrain/Terrain.hpp"

namespace OneGame::Engine::Terrain
{
void TerrainService::Initialize(const TerrainDesc& desc, Point3 chunkOrigin)
{
    m_terrainGenerator.SetTerrainGenChunkBudget(desc.terrainGenChunkBudget);
    m_terrainMeshBuilder.SetVertexBudget(desc.meshingQuadBudget);
    m_terrainUploader.SetMaxNumChunks((desc.chunkViewDistance + 1) * (desc.chunkViewDistance + 1) * 6);
    m_terrainUpdateScheduler.SetChunkViewDistance(desc.chunkViewDistance);
    m_terrainUpdateScheduler.InitialUpdate(m_terrainData, chunkOrigin);
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

void TerrainService::Update(BlockRegistry& blocks, Point3 chunkOrigin)
{
    m_terrainGenerator.GenerateTerrain(m_terrainData, blocks);
}

void TerrainService::Present(BlockRegistry& blocks, std::array<math::vec3, 6> frustum,
                             StreamingManager& sm, entt::registry& presentationWorld)
{
    m_terrainUpdateScheduler.QueueChunksForMeshing(m_terrainData, m_terrainPData);
    m_terrainMeshBuilder.BuildChunkMeshes(m_terrainData, blocks, m_terrainPData);
    m_terrainUploader.UploadTerrain(m_terrainPData, sm);
    m_terrainUpdateScheduler.UpdateChunkVisibility(m_terrainData, m_terrainPData, frustum, presentationWorld);
}

void TerrainService::UploadBuiltChunks(StreamingManager& stream)
{
    m_terrainUploader.UploadTerrain(m_terrainPData, stream);
}

GPUBufferHandle TerrainService::GetOrCreateTerrainMesh(Graphics::IGraphicsBackend& backend)
{
    if (!m_terrainPData.terrainMesh.IsValid()) m_terrainUploader.CreateTerrainMesh(m_terrainPData, backend);
    return m_terrainPData.terrainMesh;
}
}  // namespace OneGame::Engine::Terrain
