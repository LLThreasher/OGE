#include "Engine/ECS/Components.hpp"
#include "Engine/Formaters.hpp"
#include "Engine/GameAppState.hpp"
#include "Engine/Graphics/PresentationObjects.hpp"
#include "Engine/Graphics/Renderer.hpp"
#include "Engine/Graphics/SubmissionQueue.hpp"
#include "Engine/Logger.hpp"
#include "Engine/StreamingManager.hpp"
#include "Engine/Terrain/Terrain.hpp"

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

void TerrainUpdateScheduler::QueueChunksForMeshing(const TerrainData& terrain, TerrainPresentationData& pdata,
                                                   entt::dispatcher& events)
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

struct FrustumPlane
{
    glm::vec3 normal;
    float distance;

    float DistanceToPoint(const glm::vec3& p) const { return glm::dot(normal, p) + distance; }
};

struct Frustum
{
    FrustumPlane planes[5];  // left, right, bottom, top, near
};

static Frustum BuildFrustumGeometric(const ECS::ComponentCamera& cam, const ECS::ComponentPerspectiveCamera& pcam)
{
    constexpr float nearPlane = 0.1f;

    Frustum f;

    glm::vec3 pos = cam.position;
    glm::vec3 forward = cam.forward;
    glm::vec3 right = cam.right();
    glm::vec3 up    = cam.up();

    float tanHalfFov = std::tan(pcam.fov * 0.5f);

    float nearHeight = 2.0f * tanHalfFov * nearPlane;
    float nearWidth = nearHeight * pcam.aspect;

    glm::vec3 nearCenter = pos + forward * nearPlane;

    glm::vec3 nearRight = right * (nearWidth * 0.5f);
    glm::vec3 nearUp = up * (nearHeight * 0.5f);

    // 4 corners of near plane
    glm::vec3 ntl = nearCenter + nearUp - nearRight;
    glm::vec3 ntr = nearCenter + nearUp + nearRight;
    glm::vec3 nbl = nearCenter - nearUp - nearRight;
    glm::vec3 nbr = nearCenter - nearUp + nearRight;

    auto MakePlane = [](const glm::vec3& a, const glm::vec3& b, const glm::vec3& c)
    {
        FrustumPlane p;
        p.normal = glm::normalize(glm::cross(b - a, c - a));
        p.distance = -glm::dot(p.normal, a);
        return p;
    };

    // Order matters — normals must point inward

    // Left
    f.planes[0] = MakePlane(pos, nbl, ntl);

    // Right
    f.planes[1] = MakePlane(pos, ntr, nbr);

    // Bottom
    f.planes[2] = MakePlane(pos, nbr, nbl);

    // Top
    f.planes[3] = MakePlane(pos, ntl, ntr);

    // Near
    f.planes[4].normal = forward;
    f.planes[4].distance = -glm::dot(forward, nearCenter);

    return f;
}

static bool IsAABBVisible(const Frustum& frustum, const glm::vec3& min, const glm::vec3& max)
{
    for (int i = 0; i < 5; i++)
    {
        const auto& plane = frustum.planes[i];

        glm::vec3 positive;
        positive.x = (plane.normal.x >= 0.f) ? max.x : min.x;
        positive.y = (plane.normal.y >= 0.f) ? max.y : min.y;
        positive.z = (plane.normal.z >= 0.f) ? max.z : min.z;

        if (plane.DistanceToPoint(positive) < 0.f) return false;
    }

    return true;
}

static bool IsVisibleToPlayer(Point3 chunkCoord, const Frustum& frustum)
{
    glm::vec3 chunkMin =
        glm::vec3(chunkCoord.x * CHUNK_SIZE_X, chunkCoord.y * CHUNK_SIZE_Y, chunkCoord.z * CHUNK_SIZE_Z);

    glm::vec3 chunkMax = chunkMin + math::vec3(CHUNK_SIZE_X, CHUNK_SIZE_Y, CHUNK_SIZE_Z);

    return IsAABBVisible(frustum, chunkMin, chunkMax);
}

void TerrainUpdateScheduler::SubmitVisibleChunks(const TerrainData& data, TerrainPresentationData& pdata,
                                                 const TerrainContext& tctx, FrameOutputData& fd)
{
    using namespace Graphics;

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
    
    using namespace ECS;

    for (auto [handle, slot] : pdata.residentChunks)
    {
        auto chunk = data.chunks.Get(handle);

        fd.graphicQueue.Add<CmdDrawTerrainMeshOpaque>(GameViewType{baseView}, slot, chunk->Coords);
    }

    for (auto player : tctx.view<ECS::ComponentPlayer>())
    {
        auto& cam = tctx.get<ComponentCamera>(player);
        auto& pcam = tctx.get<ComponentPerspectiveCamera>(player);
        Frustum frustum = BuildFrustumGeometric(cam, pcam);

        auto it = playerToView.find(player);
        if (it == playerToView.end()) continue;

        for (auto [handle, slot] : pdata.residentChunks)
        {
            auto chunk = data.chunks.Get(handle);
            if (!IsVisibleToPlayer(chunk->Coords, frustum)) continue;
            fd.graphicQueue.Add<CmdDrawTerrainMeshOpaque>(GameViewType{it->second}, slot, chunk->Coords);
        }
    }
}
}  // namespace OneGame::Engine::Terrain