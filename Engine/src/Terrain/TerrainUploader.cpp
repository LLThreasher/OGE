#include "Engine/Graphics/PresentationObjects.hpp"
#include "Engine/StreamingManager.hpp"
#include "Engine/Terrain/Terrain.hpp"
#include "Engine/GameAppState.hpp"
#include "Engine/Graphics/Renderer.hpp"

namespace OneGame::Engine::Terrain
{
void TerrainUploader::UploadTerrain(TerrainPresentationData& terrain, PresentationContext& ctx)
{
    while (!terrain.uploadMeshQueue.empty())
    {
        auto [chunk, chunkMesh] = std::move(terrain.uploadMeshQueue.front());
        terrain.uploadMeshQueue.pop();
        size_t quadCount = terrain.builtChunkMeshes.Get(chunkMesh)->quads.size();
        auto chunkByteSize = quadCount * sizeof(TexturedQuad);
        auto slot = ctx.renderer.AllocateTerrainMesh(chunkByteSize);
        auto resolved = ctx.renderer.ResolveTerrainMesh(ctx.backend, slot);
        Graphics::PTerrainMesh pterrain {slot, static_cast<uint32_t>(quadCount * 6)};

        ResourceBundleHandle res = ctx.streamingManager.CreateResourceBundle(
            [chunk, chunkMesh, pterrain, &terrain]()
            {
                terrain.residentChunks.emplace(chunk, pterrain);
                terrain.builtChunkMeshes.Destroy(chunkMesh);
            });

        auto mesh = terrain.builtChunkMeshes.Get(chunkMesh);
        ctx.streamingManager.UploadBuffer<UploadType::Async, Graphics::BufferUsage::Storage>(mesh->quads, resolved.buffer, resolved.offset, res);
    }
}

void TerrainUploader::SetMaxNumChunks(uint32_t maxNumChunks)
{
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
                                                   std::array<math::vec3, 6> frustum, entt::registry& presentationWorld, Graphics::PGameViewTag view)
{
    for (auto [handle, slot] : pdata.residentChunks)
    {
        auto chunk = data.chunks.Get(handle);
        auto chunkEntity = presentationWorld.create();
        presentationWorld.emplace<Graphics::PGameViewTag>(chunkEntity, view);
        presentationWorld.emplace<Graphics::PTerrainMesh>(chunkEntity, slot);
        presentationWorld.emplace<Point3>(chunkEntity, chunk->Coords);
    }
}
}  // namespace OneGame::Engine::Terrain