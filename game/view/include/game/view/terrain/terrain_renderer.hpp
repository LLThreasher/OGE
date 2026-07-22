#pragma once

#include <array>
#include <cassert>
#include <optional>
#include <queue>
#include <unordered_map>
#include <unordered_set>

#include "defs_ext.hpp"
#include "game/terrain/block_registry.hpp"
#include "game/terrain/terrain_view.hpp"
#include "game/view/gfx/commands.hpp"
#include "game/view/renderer.hpp"
#include "game/view/submission_queue.hpp"
#include "oge/macros.hpp"
#include "oge/math.hpp"
#include "oge/pool.hpp"
#include "oge/runtime/entt.hpp"

#define USE_TERRAIN_MESH_V2

namespace oge
{
namespace graphics
{
class IGraphicsBackend;
class DynamicChunkAllocator;
}  // namespace graphics
namespace runtime
{
class StreamingManager;
class AssetContext;
struct PTerrainMesh;

}  // namespace runtime
}  // namespace oge

namespace game::view
{
namespace terrain
{
using namespace oge::runtime;
using namespace ::game::terrain;

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
    Pool<TerrainObject::MeshingWorkerContext, ChunkMeshingWorkerContext> meshingWorkerContexts;
    Pool<TerrainObject::BuiltChunkMesh, BuiltChunkMesh2> builtChunkMeshes;

    std::queue<std::tuple<ChunkHandle, BuiltMeshHandle>> uploadMeshQueue;
    std::unordered_map<ChunkHandle, PTerrainMesh, HandleHash<ChunkHandle>> residentChunks;
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
    using View = oge::SubmissionView<gfx::CmdDrawTerrainMeshOpaque>;

   public:
    void QueueChunksForMeshing(const TerrainData& terrain, TerrainPresentationData& pdata, entt::dispatcher& events);
    void SubmitVisibleChunks(const TerrainData& data, TerrainPresentationData& pdata, const entt::registry& tctx,
                             ViewSubmissionGroup<View> fd);

   private:
    std::unordered_map<entt::entity, uint32_t> playerToView;
    std::vector<ChunkHandle> toRemove;
    std::vector<ChunkHandle> toMesh;
};

class TerrainUploader
{
   public:
    void SetMaxNumChunks(uint32_t maxNumChunks);
    void UploadTerrain(TerrainPresentationData& terrain, AssetContext& ctx);
};

class TerrainRenderer : public Renderer
{
   public:
    TerrainRenderer() {}
    DECL_ID(TerrainRenderer);
    NO_COPY(TerrainRenderer);
    ~TerrainRenderer() = default;

    void onAttach(RendererState& ctx) override;
    void onDetach(RendererState& ctx) override;
    void onUpdate(FRendererState& ctx) override;

   private:
    TerrainPresentationData m_terrainPData;
    TerrainMeshBuilder m_terrainMeshBuilder;
    TerrainUploader m_terrainUploader;
    TerrainMeshScheduler m_terrainMeshScheduler;
};
}
using TerrainRenderer = terrain::TerrainRenderer;
}  // namespace game::view::terrain
