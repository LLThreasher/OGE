#include <limits>

#include "Engine/Terrain/BlockManager.hpp"
#include "Engine/Terrain/Terrain.hpp"
#include "Engine/GameAppState.hpp"

namespace OneGame::Engine::Terrain
{
using namespace ECS;

void TerrainService::Initialize(TerrainContext& ctx, AppContext actx)
{
    auto [entity, desc] = *ctx.world.view<TerrainDesc>().each().begin();
    m_terrainGenerator.SetTerrainGenChunkBudget(desc.terrainGenChunkBudget);
    m_terrainMeshBuilder.SetVertexBudget(desc.meshingQuadBudget);
    m_terrainUploader.SetMaxNumChunks((desc.chunkViewDistance + 1) * (desc.chunkViewDistance + 1) * 6);
    m_terrainUpdateScheduler.SetChunkViewDistance(desc.chunkViewDistance);
    ctx.world.on_construct<ComponentPlayer>().connect<&TerrainService::onPlayerCreated>(this);
}

void TerrainService::onPlayerCreated(entt::registry& world, entt::entity entity)
{
    auto pos = world.get<ComponentCamera>(entity).position;
    Point3 ipos = {math::floor(pos.x) / CHUNK_SIZE_X, math::floor(pos.y) / CHUNK_SIZE_Y, math::floor(pos.z) / CHUNK_SIZE_Z};
    m_terrainUpdateScheduler.InitialUpdate(m_terrainData, ipos);
}

void TerrainService::Update(TerrainContext& ctx, AppContext actx, const FrameInputData& fd)
{
    m_terrainGenerator.GenerateTerrain(m_terrainData, ctx.blocks);
}

void TerrainService::Present(const TerrainContext& ctx, PresentationContext pctx, FrameOutputData& frameOut)
{
    m_terrainUpdateScheduler.QueueChunksForMeshing(m_terrainData, m_terrainPData);
    m_terrainMeshBuilder.BuildChunkMeshes(m_terrainData, ctx.blocks, m_terrainPData);
    m_terrainUploader.UploadTerrain(m_terrainPData, pctx);
    m_terrainUpdateScheduler.SubmitVisibleChunks(m_terrainData, m_terrainPData, ctx, frameOut);
}

uint32_t TerrainView::GetBlock(int x, int y, int z)
{
    Point3 chunkCoord = {x >> 4, y >> 4, z >> 4};
    auto [_, chunk] = GetChunk(chunkCoord);
    assert(chunk != nullptr && chunk->state == ChunkState::Persistent);
    if (chunk != nullptr) return chunk->GetBlock(x & 0xF, y & 0xF, z & 0xF);
    return 0;
}

bool TerrainView::TryGetBlock(int x, int y, int z, uint32_t& value)
{
    Point3 chunkCoord = {x >> 4, y >> 4, z >> 4};
    auto [_, chunk] = GetChunk(chunkCoord);
    if (chunk != nullptr)
    {
        value = chunk->GetBlock(x & 0xF, y & 0xF, z & 0xF);
        return true;
    }
    return false;
}

void TerrainView::SetBlock(int x, int y, int z, uint32_t value)
{
    Point3 chunkCoord = {x >> 4, y >> 4, z >> 4};
    auto [handle, chunk] = GetChunk(chunkCoord);
    assert(chunk != nullptr);
    if (chunk != nullptr)
    {
        m_terrainData.dirtyChunks.insert(handle);
        if ((x & 0xF) == 0)
        {
            m_terrainData.dirtyChunks.insert(m_terrainData.chunks.GetHandle({chunkCoord.x - 1, chunkCoord.y, chunkCoord.z}));
        }
        else if ((x & 0xF) == 15)
        {
            m_terrainData.dirtyChunks.insert(m_terrainData.chunks.GetHandle({chunkCoord.x + 1, chunkCoord.y, chunkCoord.z}));
        }
        if ((y & 0xF) == 0)
        {
            m_terrainData.dirtyChunks.insert(m_terrainData.chunks.GetHandle({chunkCoord.x, chunkCoord.y - 1, chunkCoord.z}));
        }
        else if ((y & 0xF) == 15)
        {
            m_terrainData.dirtyChunks.insert(m_terrainData.chunks.GetHandle({chunkCoord.x, chunkCoord.y + 1, chunkCoord.z}));
        }
        if ((z & 0xF) == 0)
        {
            m_terrainData.dirtyChunks.insert(m_terrainData.chunks.GetHandle({chunkCoord.x, chunkCoord.y, chunkCoord.z - 1}));
        }
        else if ((z & 0xF) == 15)
        {
            m_terrainData.dirtyChunks.insert(m_terrainData.chunks.GetHandle({chunkCoord.x, chunkCoord.y, chunkCoord.z + 1}));
        }
        return chunk->SetBlock(x & 0xF, y & 0xF, z & 0xF, value);
    }
}

std::tuple<ChunkHandle, ChunkData*> TerrainView::GetChunk(Point3 chunkCoord)
{
    return m_terrainData.chunks.Get(chunkCoord);
}

ChunkData* TerrainView::GetChunk(ChunkHandle handle) { return m_terrainData.chunks.Get(handle); }

void TerrainView::SubmitChunk(ChunkHandle handle) { m_terrainData.dirtyChunks.insert(handle); }

std::optional<TerrainRaycastResult> TerrainView::CastRay(math::vec3 pos, math::vec3 ray, float maxDist)
{
    constexpr float MAX_INT = static_cast<float>(std::numeric_limits<int>::max());
    math::vec3 delta(ray.x == 0 ? MAX_INT : math::abs(1 / ray.x), ray.y == 0 ? MAX_INT : math::abs(1 / ray.y),
                     ray.z == 0 ? MAX_INT : math::abs(1 / ray.z));
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
        uint32_t value;
        if (!TryGetBlock(map.x, map.y, map.z, value)) break;
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
