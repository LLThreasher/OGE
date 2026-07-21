#include "game/components.hpp"
#include "game/view/terrain/terrain_renderer.hpp"
#include "oge/graphics/objects.hpp"
#include "oge/log.hpp"
#include "oge/runtime/streaming_manager.hpp"
#include "oge/runtime/asset_ctx.hpp"
#include "oge/runtime/gfx/chunk_allocator2.hpp"

namespace game::view::terrain
{
using namespace oge::graphics;

void TerrainUploader::UploadTerrain(TerrainPresentationData& terrain, AssetContext& ctx)
{
    while (!terrain.uploadMeshQueue.empty())
    {
        auto [chunk, chunkMesh] = std::move(terrain.uploadMeshQueue.front());
        terrain.uploadMeshQueue.pop();
        size_t quadCount = terrain.builtChunkMeshes.Get(chunkMesh)->quads.size();
        auto chunkByteSize = quadCount * sizeof(TexturedQuad);
        auto slot = ctx.chunkAllocator.Allocate(ctx.backend, chunkByteSize);
        auto resolved = ctx.chunkAllocator.Resolve(slot);
        PTerrainMesh pterrain{slot, static_cast<uint32_t>(quadCount * 6)};

        ResourceBundleHandle res = ctx.streamingManager.CreateResourceBundle(
            [chunk, chunkMesh, pterrain, ctx, &terrain]()
            {
                auto it = terrain.residentChunks.find(chunk);
                if (it != terrain.residentChunks.end())
                {
                    ctx.chunkAllocator.Free(it->second.alloc);
                }
                terrain.residentChunks.insert_or_assign(chunk, pterrain);
                terrain.builtChunkMeshes.Destroy(chunkMesh);
            });

        auto mesh = terrain.builtChunkMeshes.Get(chunkMesh);
        ctx.streamingManager.UploadBuffer<UploadType::Async>(
            mesh->quads, {BufferUsage::Storage, resolved.buffer, resolved.offset}, res);
    }
}

void TerrainUploader::SetMaxNumChunks(uint32_t maxNumChunks) {}

}  // namespace OneGame::Engine::Terrain