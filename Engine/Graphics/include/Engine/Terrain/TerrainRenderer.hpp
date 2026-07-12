#pragma once

#include <array>
#include <cassert>
#include <optional>
#include <queue>
#include <unordered_map>
#include <unordered_set>

#include "Engine/ClassHelper.hpp"
#include "Engine/ECS/IRenderer.hpp"
#include "Engine/Math.hpp"
#include "Engine/ObjectType.hpp"
#include "Engine/ResourcePool.hpp"
#include "Engine/Terrain/BlockManager.hpp"
#include "Engine/Terrain/TerrainVertexFormat.hpp"
#include "Engine/Terrain/TerrainView.hpp"
#include "Engine/entt.hpp"

#define USE_TERRAIN_MESH_V2

namespace OneGame::Engine
{
class StreamingManager;
struct PresentationContext;
struct FrameOutputData;
namespace ECS
{
using TerrainContext = entt::registry;
};

namespace Graphics
{
class IGraphicsBackend;
class PTerrainMesh;
namespace DCA
{
class DynamicChunkAllocator;
}
}  // namespace Graphics
}  // namespace OneGame::Engine

namespace OneGame::Engine::Terrain
{
using TerrainContext = ECS::TerrainContext;

struct BuiltChunkMesh2
{
    std::vector<TexturedQuad> quads;
};

struct BuiltMesh
{
    ChunkHandle handle;
    uint32_t vertexOffset;
    uint32_t vertexSize;
    uint32_t indexOffset;
    uint32_t indexSize;
};

struct ChunkMeshingWorkerContext
{
    // 1296 bytes
    uint32_t opaqueMasks[(CHUNK_SIZE_Y + 2) * (CHUNK_SIZE_Z + 2)];
    // 5120 bytes
    PaletteCompressedChunk compressedChunk;
    ChunkHandle chunkHandle;
    BuiltMeshHandle chunkMeshHandle;
};

struct TerrainPresentationData
{
    std::queue<ChunkHandle> buildMeshQueue;
    ResourcePool<TerrainObject::MeshingWorkerContext, ChunkMeshingWorkerContext> meshingWorkerContexts;
    ResourcePool<TerrainObject::BuiltChunkMesh, BuiltChunkMesh2> builtChunkMeshes;

    std::queue<std::tuple<ChunkHandle, BuiltMeshHandle>> uploadMeshQueue;
    std::unordered_map<ChunkHandle, Graphics::PTerrainMesh, HandleHash<ChunkHandle>> residentChunks;
};

class TerrainMeshBuilder
{
   public:
    void BuildChunkMeshes(const TerrainData& terrain, const BlockRegistry& blocks,
                          TerrainPresentationData& terrainPData);
    void SetVertexBudget(uint32_t val) { m_vertexBudget = val; }

   private:
    void ExecuteBuildChunkMesh(TerrainPresentationData& pData, MeshingWorkerContextHandle handle,
                               const BlockRegistry& blocks);

    uint32_t m_vertexBudget = 1024;
    uint32_t m_runningVertexCount = 0;
};

class TerrainMeshScheduler
{
   public:
    void QueueChunksForMeshing(const TerrainData& terrain, TerrainPresentationData& pdata, entt::dispatcher& events);
    void SubmitVisibleChunks(const TerrainData& data, TerrainPresentationData& pdata, const TerrainContext& tctx,
                             FrameOutputData& fd);

   private:
    std::unordered_map<entt::entity, uint32_t> playerToView;
    std::vector<ChunkHandle> toRemove;
    std::vector<ChunkHandle> toMesh;
};

class TerrainUploader
{
   public:
    void SetMaxNumChunks(uint32_t maxNumChunks);
    void UploadTerrain(TerrainPresentationData& terrain, PresentationContext& ctx);
};

struct TerrainRendererDesc
{
    int meshingQuadBudget = 4096 * 4;
};

class TerrainRenderer : public ECS::RendererBase
{
   public:
    NO_COPY(TerrainRenderer);
    ~TerrainRenderer() = default;

    void Initialize(TerrainContext& ctx, PresentationContext& actx) override;
    void Present(const TerrainContext& ctx, PresentationContext& actx, FrameOutputData& fd) override;

   private:
    TerrainPresentationData m_terrainPData;
    TerrainMeshBuilder m_terrainMeshBuilder;
    TerrainUploader m_terrainUploader;
    TerrainMeshScheduler m_terrainMeshScheduler;
};
}  // namespace OneGame::Engine::Terrain
