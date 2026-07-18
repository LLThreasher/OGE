#include "game/terrain/terrain_view.hpp"

#include "game/terrain/block_registry.hpp"

namespace game::terrain
{
void TerrainView::HandleResolveDirtyChunk(entt::registry& world, ResolveDirtyChunkEvent e)
{
    world.ctx().get<TerrainView>().m_terrainData.dirtyChunks.erase(e.chunk);
}

uint32_t TerrainView::GetBlock(int x, int y, int z) const
{
    Point3 chunkCoord = {x >> 4, y >> 4, z >> 4};
    auto [_, chunk] = GetChunk(chunkCoord);
    assert(chunk != nullptr && chunk->state == ChunkState::Persistent);
    if (chunk != nullptr) return chunk->GetBlock(x & 0xF, y & 0xF, z & 0xF);
    return 0;
}

bool TerrainView::TryGetBlock(int x, int y, int z, uint32_t& value) const
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
            m_terrainData.dirtyChunks.insert(
                m_terrainData.chunks.GetHandle({chunkCoord.x - 1, chunkCoord.y, chunkCoord.z}));
        }
        else if ((x & 0xF) == 15)
        {
            m_terrainData.dirtyChunks.insert(
                m_terrainData.chunks.GetHandle({chunkCoord.x + 1, chunkCoord.y, chunkCoord.z}));
        }
        if ((y & 0xF) == 0)
        {
            m_terrainData.dirtyChunks.insert(
                m_terrainData.chunks.GetHandle({chunkCoord.x, chunkCoord.y - 1, chunkCoord.z}));
        }
        else if ((y & 0xF) == 15)
        {
            m_terrainData.dirtyChunks.insert(
                m_terrainData.chunks.GetHandle({chunkCoord.x, chunkCoord.y + 1, chunkCoord.z}));
        }
        if ((z & 0xF) == 0)
        {
            m_terrainData.dirtyChunks.insert(
                m_terrainData.chunks.GetHandle({chunkCoord.x, chunkCoord.y, chunkCoord.z - 1}));
        }
        else if ((z & 0xF) == 15)
        {
            m_terrainData.dirtyChunks.insert(
                m_terrainData.chunks.GetHandle({chunkCoord.x, chunkCoord.y, chunkCoord.z + 1}));
        }
        return chunk->SetBlock(x & 0xF, y & 0xF, z & 0xF, value);
    }
}

std::tuple<ChunkHandle, const ChunkData*> TerrainView::GetChunk(Point3 chunkCoord) const
{
    return m_terrainData.chunks.Get(chunkCoord);
}

std::tuple<ChunkHandle, ChunkData*> TerrainView::GetChunk(Point3 chunkCoord)
{
    return m_terrainData.chunks.Get(chunkCoord);
}

const ChunkData* TerrainView::GetChunk(ChunkHandle handle) const { return m_terrainData.chunks.Get(handle); }
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
    int lastDim = -1;
    while (dist < maxDist)
    {
        size_t dim;

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
        if (side.z < temp) dim = 2;

        // step first
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

        lastDim = dim;

        uint32_t value;
        if (!TryGetBlock(map.x, map.y, map.z, value)) break;

        if (BlockRegistry::GetBlockId(value) != 0)
        {
            uint8_t face = (lastDim == 0)   ? (step.x > 0 ? 1 : 0)
                           : (lastDim == 1) ? (step.y > 0 ? 3 : 2)
                                            : (step.z > 0 ? 5 : 4);

            return TerrainRaycastResult{face, map, value};
        }

        dist++;
    }
    return {};
}

}