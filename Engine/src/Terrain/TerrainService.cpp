#include "Engine/Terrain/BlockManager.hpp"
#include "Engine/Terrain/Terrain.hpp"
#include <limits>

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

std::optional<TerrainRaycastResult> TerrainService::CastRay(math::vec3 pos, math::vec3 ray, float maxDist)
{
    constexpr int MAX_INT = std::numeric_limits<int>::max();
    math::vec3 delta(ray.x == 0 ? MAX_INT : math::abs(1 / ray.x), ray.y == 0 ? INT_MAX : math::abs(1 / ray.y), ray.z == 0 ? INT_MAX : math::abs(1 / ray.z));
    Point3 map = {math::floor(pos.x), math::floor(pos.y), math::floor(pos.z)};
    Point3 step;
    math::vec3 side;
    if (ray.x < 0)
    {
        step.x = -1;
        side.x = (pos.x - map.x) * delta.x;
    }
    else
    {
        step.x = 1;
        side.x = (map.x + 1 - pos.x) * delta.x;
    }
    if (ray.y < 0)
    {
        step.y = -1;
        side.y = (pos.y - map.y) * delta.y;
    }
    else
    {
        step.y = 1;
        side.y = (map.y + 1 - pos.y) * delta.y;
    }
    if (ray.z < 0)
    {
        step.z = -1;
        side.z = (pos.z - map.z) * delta.z;
    }
    else
    {
        step.z = 1;
        side.z = (map.z + 1 - pos.z) * delta.z;
    }
    int dist = 0;
    int dim = 0;
    while (dist < maxDist)
    {
        auto value = GetBlock(map.x, map.y, map.z);
        if (BlockRegistry::GetBlockId(value) != 0)
        {
            TerrainRaycastResult res {
                (uint8_t)(dim + (step[dim] + 1) * 3 / 2),
                map,
                value,
            };
            return res;
        }

        size_t dim;
        {
            float temp;
            if (side.x < side.y)
            {
                temp = side.x;
                dim = 0;
            }
            else
            {
                temp = side.y;
                dim = 1;
            }
            if (side.z < temp)
                dim = 2;
        }
        switch (dim)
        {
            case 0:
                map.x += step.x;
                side.x += delta.x;
                break;
            case 1:
                map.y += step.y;
                side.y += delta.y;
                break;
            case 2:
                map.z += step.z;
                side.z += delta.z;
                break;
        }
        dist++;
    }
    return {};
}
}  // namespace OneGame::Engine::Terrain
