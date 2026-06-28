#pragma once

#include <array>
#include <cassert>
#include <queue>
#include <unordered_map>
#include <unordered_set>

#include "BlockManager.hpp"
#include "Engine/ClassHelper.hpp"
#include "Engine/Graphics/ChunkAllocator.hpp"
#include "Engine/Math.hpp"
#include "Engine/ObjectType.hpp"
#include "Engine/ResourcePool.hpp"
#include "Engine/Terrain/TerrainVertexFormat.hpp"

#define USE_TERRAIN_MESH_V2

namespace OneGame::Engine
{
class StreamingManager;

namespace Graphics
{
class IGraphicsBackend;
}
}  // namespace OneGame::Engine

namespace OneGame::Engine::Terrain
{
enum class TerrainObject
{
    Chunk,
    BuiltChunkMesh,
    MeshingWorkerContext,
};

using ChunkHandle = ResourceHandle<TerrainObject::Chunk>;
using BuiltMeshHandle = ResourceHandle<TerrainObject::BuiltChunkMesh>;
using MeshingWorkerContextHandle = ResourceHandle<TerrainObject::MeshingWorkerContext>;

struct LocalPoint3
{
    int8_t x, y, z;
};

struct Point3
{
    int32_t x, y, z;

    bool operator==(const Point3& other) const noexcept { return x == other.x && y == other.y && z == other.z; }

    Point3 operator+(const Point3& other) const noexcept { return {x + other.x, y + other.y, z + other.z}; }

    Point3 operator-(const Point3& other) const noexcept { return {x - other.x, y - other.y, z - other.z}; }
};

struct Point3Hash
{
    size_t operator()(const Point3& p) const noexcept
    {
        size_t hx = std::hash<int32_t>{}(p.x);
        size_t hy = std::hash<int32_t>{}(p.y);
        size_t hz = std::hash<int32_t>{}(p.z);

        // Mix the hashes
        size_t seed = hx;
        seed ^= hy + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        seed ^= hz + 0x9e3779b9 + (seed << 6) + (seed >> 2);

        return seed;
    }
};

enum class ChunkState
{
    GeneratingTerrain,
    GeneratingMesh,
    PendingUpload,
    GpuAvailable,
    PendingDestroy,
};

inline size_t GetBlockIndex(uint8_t x, uint8_t y, uint8_t z)
{
    return ((size_t)x << CHUNK_SHIFT_X) + ((size_t)y << CHUNK_SHIFT_Y) + ((size_t)z << CHUNK_SHIFT_Z);
}

struct BuiltChunkMesh
{
    std::vector<Vertex> vertices;
    std::vector<uint16_t> indices;
};

struct BuiltChunkMesh2
{
    std::vector<TexturedQuad> quads;
};

struct AllocatedChunkSlot
{
    uint32_t index;
    uint32_t size;  // always 1, 2, 4

    bool operator==(const AllocatedChunkSlot& other) const noexcept
    {
        return index == other.index && size == other.size;
    }
};

struct AllocatedChunkSlotHasher
{
    std::size_t operator()(const AllocatedChunkSlot& slot) const noexcept
    {
        return (static_cast<uint64_t>(slot.index) << 3) | slot.size;
    }
};

struct ChunkData
{
    uint32_t data[CHUNK_SIZE_TOTAL] = {};
    Point3 Coords = {};

   public:
    ChunkData(Point3 coords) { Coords = coords; }

    uint32_t GetBlock(uint8_t x, uint8_t y, uint8_t z)
    {
        assert(0 <= x && x < CHUNK_SIZE_X);
        assert(0 <= y && y < CHUNK_SIZE_Y);
        assert(0 <= z && z < CHUNK_SIZE_Z);
        return data[GetBlockIndex(x, y, z)];
    }

    void SetBlock(uint8_t x, uint8_t y, uint8_t z, uint32_t value) { data[GetBlockIndex(x, y, z)] = value; }
};

struct LocalUpdateBlockCmd
{
    uint32_t value;
    LocalPoint3 coord;
    bool touchesBorder;
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
    // 8192 bytes
    BlockMetadata blockMetadata[CHUNK_SIZE_TOTAL];
    ChunkHandle chunkHandle;
    BuiltMeshHandle chunkMeshHandle;
};

class ChunkDataCollection
{
   public:
    const ChunkData* Get(ChunkHandle chunk) const { return chunkData.Get(chunk); }

    std::tuple<ChunkHandle, const ChunkData*> Get(Point3 coord) const
    {
        auto it = coordToChunks.find(coord);
        if (it != coordToChunks.end())
        {
            return {it->second, Get(it->second)};
        }
        return {{}, nullptr};
    }

    ChunkData* Get(ChunkHandle chunk) { return chunkData.Get(chunk); }

    std::tuple<ChunkHandle, ChunkData*> Get(Point3 coord)
    {
        auto it = coordToChunks.find(coord);
        if (it != coordToChunks.end())
        {
            return {it->second, Get(it->second)};
        }
        return {{}, nullptr};
    }

    ChunkHandle AllocateChunk(Point3 coord)
    {
        auto it = coordToChunks.find(coord);
        if (it != coordToChunks.end())
        {
            return it->second;
        }
        auto res = chunkData.Create(coord);
        coordToChunks.emplace(coord, res);
        return res;
    }

    void FreeChunk(Point3 coord)
    {
        auto it = coordToChunks.find(coord);
        if (it != coordToChunks.end())
        {
            chunkData.Destroy(it->second);
            coordToChunks.erase(it);
        }
    }

    void FreeChunk(ChunkHandle handle)
    {
        auto data = chunkData.Get(handle);
        coordToChunks.erase(data->Coords);
        chunkData.Destroy(handle);
    }

   private:
    ResourcePool<TerrainObject::Chunk, ChunkData> chunkData;
    std::unordered_map<Point3, ChunkHandle, Point3Hash> coordToChunks;
};

// allocate chunk -> generate terrain queue -> build mesh queue -> built chunk
// meshes -> upload with streaming manager -> remove built chunk meshes ->
// resident chunk any state -> destroy chunk
struct TerrainData
{
    ChunkDataCollection chunks;
    std::queue<ChunkHandle> generateTerrainQueue;
    std::unordered_set<Point3, Point3Hash> chunksToDestroy;
    std::unordered_map<ChunkHandle, std::vector<LocalUpdateBlockCmd>, HandleHash<ChunkHandle>> blockModificationQueue;
    std::unordered_set<ChunkHandle, HandleHash<ChunkHandle>> dirtyChunks;
};

struct TerrainPresentationData
{
    std::queue<ChunkHandle> buildMeshQueue;
    ResourcePool<TerrainObject::MeshingWorkerContext, ChunkMeshingWorkerContext> meshingWorkerContexts;
    ResourcePool<TerrainObject::BuiltChunkMesh, BuiltChunkMesh2> builtChunkMeshes;

    std::queue<std::tuple<ChunkHandle, BuiltMeshHandle>> uploadMeshQueue;
    GPUBufferHandle terrainMesh;

    std::unordered_set<ChunkHandle, HandleHash<ChunkHandle>> residentChunks;
    std::unordered_set<AllocatedChunkSlot, AllocatedChunkSlotHasher> currentVisibleChunks;
};

void ExecuteBuildChunkMeshJob(const ChunkMeshingWorkerContext* _context, BuiltChunkMesh* context);

class TerrainMeshBuilder
{
   public:
    void BuildChunkMeshes(const TerrainData& terrain, const BlockRegistry& blocks,
                          TerrainPresentationData& terrainPData);
    void SetVertexBudget(uint32_t val) { m_vertexBudget = val; }

   private:
    void ExecuteBuildChunkMesh(TerrainPresentationData& pData, MeshingWorkerContextHandle);

    uint32_t m_vertexBudget = 1024;
    uint32_t m_runningVertexCount = 0;
};

class TerrainUpdateScheduler
{
   public:
    void InitialUpdate(TerrainData& terrain, Point3 chunkOrigin)
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

    void UpdateChunkVisibility(TerrainData& terrain, Point3 chunkOrigin, std::array<math::vec3, 6> frustum) {}

    void SetChunkViewDistance(int val) { m_chunkViewDistance = val; }

   private:
    int m_chunkViewDistance = 4;
};

class TerrainUploader
{
   public:
    void SetMaxNumChunks(uint32_t maxNumChunks);
    void UploadTerrain(TerrainPresentationData& terrain, StreamingManager& sm);

   private:
    Graphics::ChunkAllocator m_chunkAllocator;
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
    void GenerateTerrain(TerrainData& terrain);
    void SetTerrainGenChunkBudget(int chunkBudget) { terrainGenChunkBudget = chunkBudget; }

   private:
    int terrainGenChunkBudget = 8;
};

struct VisibleChunkMeshes
{
    std::tuple<GPUBufferHandle, GPUBufferHandle> gpuBuffers;
    std::unordered_set<AllocatedChunkSlot, AllocatedChunkSlotHasher>& meshes;
};

class TerrainService
{
   public:
    NO_COPY(TerrainService);
    ~TerrainService() = default;

    void Initialize(const TerrainDesc& desc, Point3 chunkOrigin);
    uint32_t GetBlock(int x, int y, int z);
    void SetBlock(int x, int y, int z, uint32_t value);
    std::tuple<ChunkHandle, ChunkData*> GetChunk(Point3 chunkCoord);
    ChunkData* GetChunk(ChunkHandle handle);
    void SubmitChunk(ChunkHandle handle);

    void Update(Point3 chunkOrigin, std::array<math::vec3, 6> frustum);

    void BuildTerrainMesh(BlockRegistry& blocks);
    void UploadBuiltChunks(StreamingManager& stream);

   private:
    TerrainData m_terrainData;
    TerrainPresentationData m_terrainPData;
    TerrainGenerator m_terrainGenerator;
    TerrainMeshBuilder m_terrainMeshBuilder;
    TerrainUpdateScheduler m_terrainUpdateScheduler;
    TerrainUploader m_terrainUploader;
};
}  // namespace OneGame::Engine::Terrain
