#include "Engine/ECS/Components.hpp"
#include "Engine/Formaters.hpp"
#include "Engine/GameAppState.hpp"
#include "Engine/Graphics/PresentationObjects.hpp"
#include "Engine/Graphics/Renderer.hpp"
#include "Engine/Graphics/SubmissionQueue.hpp"
#include "Engine/Logger.hpp"
#include "Engine/StreamingManager.hpp"
#include "Engine/Terrain/TerrainRenderer.hpp"

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
        Graphics::PTerrainMesh pterrain{slot, static_cast<uint32_t>(quadCount * 6)};

        ResourceBundleHandle res = ctx.streamingManager.CreateResourceBundle(
            [chunk, chunkMesh, pterrain, ctx, &terrain]()
            {
                auto it = terrain.residentChunks.find(chunk);
                ;
                if (it != terrain.residentChunks.end())
                {
                    ctx.renderer.FreeTerrainMesh(it->second.alloc);
                }
                terrain.residentChunks.insert_or_assign(chunk, pterrain);
                terrain.builtChunkMeshes.Destroy(chunkMesh);
            });

        auto mesh = terrain.builtChunkMeshes.Get(chunkMesh);
        ctx.streamingManager.UploadBuffer<UploadType::Async, Graphics::BufferUsage::Storage>(
            mesh->quads, resolved.buffer, resolved.offset, res);
    }
}

void TerrainUploader::SetMaxNumChunks(uint32_t maxNumChunks) {}

}  // namespace OneGame::Engine::Terrain