#pragma once

#include <array>
#include <cassert>
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <optional>

#include "Engine/entt.hpp"
#include "BlockManager.hpp"
#include "Engine/ClassHelper.hpp"
#include "Engine/Graphics/ChunkAllocator.hpp"
#include "Engine/Graphics/PresentationObjects.hpp"
#include "Engine/Math.hpp"
#include "Engine/ObjectType.hpp"
#include "Engine/ResourcePool.hpp"
#include "Engine/Terrain/TerrainVertexFormat.hpp"
#include "Engine/Terrain/TerrainView.hpp"
#include "Engine/ECS/ISubsystem.hpp"

#define USE_TERRAIN_MESH_V2

namespace OneGame::Engine
{
class StreamingManager;
struct PresentationContext;
struct FrameOutputData;
namespace ECS
{
struct TerrainContext;
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

struct BuiltChunkMesh
{
    std::vector<Vertex> vertices;
    std::vector<uint16_t> indices;
};

struct BuiltChunkMesh2
{
    std::vector<TexturedQuad> quads;
};

struct PaletteCompressedChunk
{
    std::vector<uint32_t> palette;
    uint8_t data[CHUNK_SIZE_TOTAL];

    static PaletteCompressedChunk FromChunkData(const ChunkData& c)
    {
        PaletteCompressedChunk result;
        std::unordered_map<uint32_t, uint8_t> palette_map;
        for (size_t i = 0; i < CHUNK_SIZE_TOTAL; ++i)
        {
            auto it = palette_map.find(c.data[i]);
            if (it == palette_map.end())
            {
                palette_map.emplace(c.data[i], result.palette.size());
                result.data[i] = result.palette.size();
                result.palette.push_back(c.data[i]);
            }
            else
            {
                result.data[i] = it->second;
            }
        }
        assert(result.palette.size() <= 255);
        return result;
    }

    uint32_t Get(int x, int y, int z) const { return palette[data[GetBlockIndex(x, y, z)]]; }
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

void ExecuteBuildChunkMeshJob(const ChunkMeshingWorkerContext* _context, BuiltChunkMesh* context,
                              const BlockRegistry& blocks);

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

class TerrainUpdateScheduler
{
   public:
    void InitialUpdate(TerrainData& terrain, Point3 chunkOrigin);
    void QueueChunksForMeshing(TerrainData& terrain, TerrainPresentationData& pdata);
    void SubmitVisibleChunks(TerrainData& data, TerrainPresentationData& pdata, const TerrainContext& tctx, FrameOutputData& fd);
    void SetChunkViewDistance(int val) { m_chunkViewDistance = val; }

   private:
    int m_chunkViewDistance = 4;
    std::unordered_map<entt::entity, uint32_t> playerToView;
};

class TerrainUploader
{
   public:
    void SetMaxNumChunks(uint32_t maxNumChunks);
    void UploadTerrain(TerrainPresentationData& terrain, PresentationContext& ctx);
};

struct TerrainDesc
{
    int chunkViewDistance = 8;
    int meshingQuadBudget = 4096 * 4;
    int terrainGenChunkBudget = 8;
};

class TerrainGenerator
{
   public:
    void GenerateTerrain(TerrainData& terrain, BlockRegistry& blocks);
    void SetTerrainGenChunkBudget(int chunkBudget) { terrainGenChunkBudget = chunkBudget; }

   private:
    int terrainGenChunkBudget = 8;
};

class TerrainService : public TerrainView, ECS::ISubsystem<TerrainContext>
{
   public:
    NO_COPY(TerrainService);
    ~TerrainService() = default;

    void Initialize(TerrainContext& ctx, AppContext actx) override;
    void Update(TerrainContext& ctx, AppContext actx, const FrameInputData& fd) override;
    void Present(const TerrainContext& ctx, PresentationContext pctx, FrameOutputData& fd) override;

   private:
    void onPlayerCreated(entt::registry& world, entt::entity entity);
    TerrainPresentationData m_terrainPData;
    TerrainGenerator m_terrainGenerator;
    TerrainMeshBuilder m_terrainMeshBuilder;
    TerrainUpdateScheduler m_terrainUpdateScheduler;
    TerrainUploader m_terrainUploader;
};
}  // namespace OneGame::Engine::Terrain
