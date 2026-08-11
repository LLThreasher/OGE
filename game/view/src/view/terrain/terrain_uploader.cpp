#include <memory>

#include "game/game_world.hpp"
#include "game/terrain/terrain_view.hpp"
#include "game/view/terrain/terrain_renderer.hpp"
#include "oge/graphics/objects.hpp"
#include "oge/log.hpp"
#include "oge/runtime/asset_ctx.hpp"
#include "oge/runtime/gfx/chunk_allocator2.hpp"
#include "oge/runtime/streaming_manager.hpp"

namespace game::view::terrain
{
using namespace oge::graphics;

void TerrainUploader::UploadTerrain(TerrainPresentationData& terrain, AssetContext& ctx, entt::dispatcher& events)
{
    while (!terrain.uploadMeshQueue.empty())
    {
        auto [chunk, chunkMesh] = std::move(terrain.uploadMeshQueue.front());
        terrain.uploadMeshQueue.pop();
        size_t quadCount =
            terrain.builtChunkMeshes.Get(chunkMesh)->quads.size();
        auto chunkByteSize = quadCount * sizeof(TexturedQuad);
        auto slot = ctx.chunkAllocator.Allocate(ctx.backend, chunkByteSize);
        auto resolved = ctx.chunkAllocator.Resolve(slot);
        PTerrainMesh pterrain{slot, static_cast<uint32_t>(quadCount * 6)};

        TerrainUploadedEvent e {chunk, chunkMesh, pterrain, &ctx, &terrain};
        ResourceBundleHandle res = ctx.streamingManager.CreateResourceBundle(
            [e, &events]()
            {
                events.enqueue(e);
            });

        auto mesh = terrain.builtChunkMeshes.Get(chunkMesh);
        ctx.streamingManager.UploadBuffer<UploadType::Async>(
            mesh->quads,
            {BufferUsage::Storage, resolved.buffer, resolved.offset}, res);
    }
}

void TerrainUploader::onTerrainUploaded(TerrainUploadedEvent e)
{
    auto it = e.terrain->residentChunks.find(e.chunk);
    if (it != e.terrain->residentChunks.end())
    {
        e.ctx->chunkAllocator.Free(it->second.alloc);
    }
    e.terrain->residentChunks.insert_or_assign(e.chunk, e.pterrain);
    e.terrain->builtChunkMeshes.Destroy(e.chunkMesh);
}

void TerrainUploader::SetMaxNumChunks(uint32_t maxNumChunks)
{
}

}  // namespace game::view::terrain