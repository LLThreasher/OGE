#include "Engine/Graphics/PresentationObjects.hpp"
#include "Engine/StreamingManager.hpp"
#include "Engine/Terrain/Terrain.hpp"
#include "Engine/GameAppState.hpp"
#include "Engine/Graphics/Renderer.hpp"
#include "Engine/Logger.hpp"
#include "Engine/Formaters.hpp"

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
            [chunk, chunkMesh, pterrain, ctx, &terrain]()
            {
                auto it = terrain.residentChunks.find(chunk);;
                if (it != terrain.residentChunks.end())
                {
                    ctx.renderer.FreeTerrainMesh(it->second.alloc);
                }
                terrain.residentChunks.insert_or_assign(chunk, pterrain);
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

void TerrainUpdateScheduler::QueueChunksForMeshing(const TerrainData& terrain, TerrainPresentationData& pdata, entt::dispatcher& events)
{
    std::vector<ChunkHandle> toRemove;
    std::vector<ChunkHandle> toMesh;
    for (auto handle : terrain.dirtyChunks)
    {
        auto chunk = terrain.chunks.Get(handle);
        if (!chunk || chunk->state != ChunkState::Persistent)
        {
            toRemove.push_back(handle);
            continue;
        }
        bool fullNeighbors = true;
        for (int i = 0; i < 6; ++i)
        {
            if (i == FACE_DOWN && chunk->Coords.y == 0) continue;
            auto neighborCoord = perFaceOffset[i] + chunk->Coords;
            auto [handle, chunk] = terrain.chunks.Get(neighborCoord);
            if (!chunk || chunk->state != ChunkState::Persistent)
            {
                fullNeighbors = false;
                break;
            }
        }
        if (!fullNeighbors) continue;
        toMesh.push_back(handle);
    }
    for (auto handle : toRemove)
    {
        events.enqueue<ResolveDirtyChunkEvent>(handle);
    }
    for (auto handle : toMesh)
    {
        pdata.buildMeshQueue.push(handle);
        events.enqueue<ResolveDirtyChunkEvent>(handle);
    }
}

static bool IsVisibleToPlayer(Point3 chunkCoord, const entt::registry& gameWorld, entt::entity player)
{
    return true;
}

void TerrainUpdateScheduler::SubmitVisibleChunks(const TerrainData& data, TerrainPresentationData& pdata, const TerrainContext& tctx, FrameOutputData& fd)
{
    playerToView.clear();
    uint32_t baseView = 0;
    for (auto [entity, view] : tctx.view<ECS::ViewPanel>().each())
    {
        if (!tctx.all_of<ECS::ComponentPlayer>(view.activeCamera))
        {
            baseView |= static_cast<uint32_t>(view.activeSlot);
        }
        else
        {
            playerToView.emplace(view.activeCamera, static_cast<uint32_t>(view.activeSlot));
        }
    }
    for (auto [handle, slot] : pdata.residentChunks)
    {
        auto chunk = data.chunks.Get(handle);
        uint32_t currentView = baseView;
        for (auto entity : tctx.view<ECS::ComponentPlayer>())
        {
            auto it = playerToView.find(entity);
            if (it != playerToView.end() && IsVisibleToPlayer(chunk->Coords, tctx, entity))
                currentView |= it->second;
        }

        auto chunkEntity = fd.presentationWorld.create();
        fd.presentationWorld.emplace<Graphics::PGameViewTag>(chunkEntity, Graphics::GameViewType{currentView});
        fd.presentationWorld.emplace<Graphics::PTerrainMesh>(chunkEntity, slot);
        fd.presentationWorld.emplace<Point3>(chunkEntity, chunk->Coords);
    }
}
}  // namespace OneGame::Engine::Terrain