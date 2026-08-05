#include "game/terrain/terrain_view.hpp"

#include <tuple>

#include "game/terrain/block_registry.hpp"
#include "oge/point3.hpp"

namespace game::terrain
{

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
    oge::CompactLocalPoint3 localCoord = oge::Point3{x & 0xF, y & 0xF, z & 0xF};
    auto [handle, chunk] = GetChunk(chunkCoord);
    assert(chunk != nullptr);

    static auto handleChunk = [&](Point3 coord)
    {
        auto nhandle = m_terrainData.chunks.GetHandle(coord);
        DowngradeChunk(nhandle, ChunkState::InvalidLighting);
        UpgradeChunk(nhandle, ChunkState::Persistent);
    };
    if (chunk != nullptr)
    {
        ChunkStateUpdateEvent e{};
        e.AddDirtyBlk(localCoord);
        DowngradeChunk(handle, ChunkState::InvalidLighting, e);
        UpgradeChunk(handle, ChunkState::Persistent, e);
        if ((x & 0xF) == 0)
        {
            handleChunk({chunkCoord.x - 1, chunkCoord.y, chunkCoord.z});
        }
        else if ((x & 0xF) == 15)
        {
            handleChunk({chunkCoord.x + 1, chunkCoord.y, chunkCoord.z});
        }
        if ((y & 0xF) == 0)
        {
            handleChunk({chunkCoord.x, chunkCoord.y - 1, chunkCoord.z});
        }
        else if ((y & 0xF) == 15)
        {
            handleChunk({chunkCoord.x, chunkCoord.y + 1, chunkCoord.z});
        }
        if ((z & 0xF) == 0)
        {
            handleChunk({chunkCoord.x, chunkCoord.y, chunkCoord.z - 1});
        }
        else if ((z & 0xF) == 15)
        {
            handleChunk({chunkCoord.x, chunkCoord.y, chunkCoord.z + 1});
        }
        return chunk->SetBlock(x & 0xF, y & 0xF, z & 0xF, value);
    }
}

std::tuple<ChunkHandle, const ChunkData*> TerrainView::GetChunk(
    Point3 chunkCoord) const
{
    return m_terrainData.chunks.Get(chunkCoord);
}

std::tuple<ChunkHandle, ChunkData*> TerrainView::GetChunk(Point3 chunkCoord)
{
    return m_terrainData.chunks.Get(chunkCoord);
}

const ChunkData* TerrainView::GetChunk(ChunkHandle handle) const
{
    return m_terrainData.chunks.Get(handle);
}

const ChunkData* TerrainView::PollChunk(ChunkHandle& handle) const
{
    return m_terrainData.chunks.Poll(handle);
}

ChunkData* TerrainView::GetChunk(ChunkHandle handle)
{
    return m_terrainData.chunks.Get(handle);
}

ChunkHandle TerrainView::CreateChunk(Point3 chunkCoord)
{
    return m_terrainData.chunks.AllocateChunk(chunkCoord);
}

static bool AllNeighborsValid(const ChunkDataCollection& chunks, ChunkState src,
                              Point3 coord)
{
    bool valid = true;

    ChunkDir::ForEachNeighbor(coord,
                              [&](const Point3& neighborPos)
                              {
                                  auto handle = chunks.GetHandle(neighborPos);
                                  if (!handle.IsValid())
                                  {
                                      valid = false;
                                      return;
                                  }

                                  if (chunks.Get(handle)->weakState < src)
                                  {
                                      valid = false;
                                  }
                              });

    return valid;
}

void TerrainView::UpgradeChunk(ChunkHandle handle, ChunkState state,
                               ChunkStateUpdateEvent dirtyPts)
{
    UpgradeChunkInternal(handle, state, true, dirtyPts);
}

void TerrainView::UpgradeChunkInternal(ChunkHandle handle, ChunkState state,
                                       bool updateNeighbors,
                                       ChunkStateUpdateEvent event)
{
    auto chunk = m_terrainData.chunks.Get(handle);
    if (!chunk) return;
    if (state <= chunk->state) return;
    if (updateNeighbors)
    {
        if (chunk->weakState < state)
        {
            chunk->weakState = state;
            chunk->weakEvent = event;
        }

        auto coord = chunk->Coords;

        ChunkDir::ForEachNeighbor(
            coord,
            [&](const Point3& neighborPos)
            {
                UpgradeChunkInternal(
                    m_terrainData.chunks.GetHandle(neighborPos), state, false);
            });
    }
    if (!AllNeighborsValid(m_terrainData.chunks, state, chunk->Coords)) return;
    if (!event.IsValid()) event = chunk->weakEvent;
    auto prevState = chunk->state;
    chunk->state = state;
    event.packed.prevState = prevState;
    event.packed.state = state;
    event.chunk = handle;
    m_chunkEvents.Push(event);
    // LOG_DEBUG("upgrade chunk {}, {}", chunk->Coords,
    // m_chunkEvents.HeadIndex());
}

void TerrainView::DowngradeChunk(ChunkHandle handle, ChunkState newState,
                                 ChunkStateUpdateEvent event)
{
    assert(newState != ChunkState::Persistent);
    auto chunk = m_terrainData.chunks.Get(handle);
    if (!chunk) return;
    if (newState >= chunk->state) return;
    auto prevState = chunk->state;
    chunk->state = newState;
    chunk->weakState = newState;
    event.packed.prevState = prevState;
    event.packed.state = newState;
    event.chunk = handle;
    m_chunkEvents.Push(event);
}

std::optional<TerrainRaycastResult> TerrainView::CastRay(math::vec3 pos,
                                                         math::vec3 ray,
                                                         float maxDist)
{
    constexpr float MAX_INT =
        static_cast<float>(std::numeric_limits<int>::max());
    math::vec3 delta(ray.x == 0 ? MAX_INT : math::abs(1 / ray.x),
                     ray.y == 0 ? MAX_INT : math::abs(1 / ray.y),
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

}  // namespace game::terrain