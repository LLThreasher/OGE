#include "Engine/Graphics/PresentationObjects.hpp"
#include "Engine/StreamingManager.hpp"
#include "Engine/Terrain/Terrain.hpp"

namespace OneGame::Engine::Terrain
{
void TerrainUploader::UploadTerrain(TerrainPresentationData& terrain, StreamingManager& sm)
{
    while (!terrain.uploadMeshQueue.empty())
    {
        auto [chunk, chunkMesh] = std::move(terrain.uploadMeshQueue.front());
        terrain.uploadMeshQueue.pop();
        size_t quadCount = terrain.builtChunkMeshes.Get(chunkMesh)->quads.size();
        auto chunkByteSize = quadCount * sizeof(TexturedQuad);
        int chunkNum = math::ceil((float)chunkByteSize / CHUNK_STORE_BYTE_SIZE);
        assert(chunkNum <= 4);
        if (chunkNum == 3) chunkNum = 4;
        auto chunkSlot = m_chunkAllocator.Allocate(chunkNum);
        AllocatedChunkSlot slot{(uint32_t)chunkSlot, (uint32_t)chunkNum, (uint32_t)quadCount * 6u};
        
        ResourceBundleHandle res = sm.CreateResourceBundle(
            [chunk, chunkMesh, slot, &terrain]()
            {
                terrain.residentChunks.emplace(chunk, slot);
                terrain.builtChunkMeshes.Destroy(chunkMesh);
            });

        auto mesh = terrain.builtChunkMeshes.Get(chunkMesh);
        sm.UploadBuffer<UploadType::Async, Graphics::BufferUsage::Storage>(mesh->quads, terrain.terrainMesh, slot.slot * CHUNK_STORE_BYTE_SIZE, res);
    }
}

void TerrainUploader::SetMaxNumChunks(uint32_t maxNumChunks)
{
    m_chunkAllocator.Initialize(math::align(maxNumChunks, 4));
}

void TerrainUploader::CreateTerrainMesh(TerrainPresentationData& terrain, Graphics::IGraphicsBackend& backend)
{
    assert(32 * 1024 * 1024 >= m_chunkAllocator.GetMaxNumChunks() * CHUNK_STORE_BYTE_SIZE);
    terrain.terrainMesh = backend.AllocateGPUBuffer<Graphics::BufferUsage::Storage>(
        32 * 1024 * 1024);  // 32 mb, 1320 chunks, 8 chunk view distance
}

void TerrainUpdateScheduler::InitialUpdate(TerrainData& terrain, Point3 chunkOrigin)
{
    for (int x = -m_chunkViewDistance - 1; x <= m_chunkViewDistance + 1; ++x)
    {
        for (int z = -m_chunkViewDistance - 1; z <= m_chunkViewDistance + 1; ++z)
        {
            for (int y = 0; y <= 4; ++y)
            {
                auto handle = terrain.chunks.AllocateChunk({x, y, z});
                terrain.generateTerrainQueue.push(handle);
            }
        }
    }
}

void TerrainUpdateScheduler::QueueChunksForMeshing(TerrainData& terrain, TerrainPresentationData& pdata)
{
    std::vector<ChunkHandle> toRemove;
    for (auto handle : terrain.dirtyChunks)
    {
        auto chunk = terrain.chunks.Get(handle);
        bool fullNeighbors = true;
        for (int i = 0; i < 6; ++i)
        {
            auto neighborCoord = perFaceOffset[i] + chunk->Coords;
            auto [handle, chunk] = terrain.chunks.Get(neighborCoord);
            if (!chunk || chunk->state != ChunkState::Persistent)
            {
                fullNeighbors = false;
                break;
            }
        }
        if (!fullNeighbors) continue;
        toRemove.push_back(handle);
    }
    for (auto handle : toRemove)
    {
        pdata.buildMeshQueue.push(handle);
        terrain.dirtyChunks.erase(handle);
    }
}

void TerrainUpdateScheduler::UpdateChunkVisibility(TerrainData& data, TerrainPresentationData& pdata,
                                                   std::array<math::vec3, 6> frustum, entt::registry& presentationWorld)
{
    for (auto [handle, slot] : pdata.residentChunks)
    {
        auto chunk = data.chunks.Get(handle);
        auto chunkEntity = presentationWorld.create();
        Graphics::PTerrainMesh mesh{
            slot.slot * CHUNK_STORE_BYTE_SIZE, slot.indexCount, chunk->Coords.x, chunk->Coords.y,
            chunk->Coords.z,
        };
        presentationWorld.emplace<Graphics::PTerrainMesh>(chunkEntity, mesh);
    }
}
}  // namespace OneGame::Engine::Terrain