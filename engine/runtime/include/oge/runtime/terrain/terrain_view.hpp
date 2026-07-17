#pragma once

#include <queue>
#include <unordered_map>
#include <unordered_set>

#include "oge/point3.hpp"
#include "oge/pool.hpp"
#include "oge/runtime/entt.hpp"
#include "oge/runtime/terrain/defs.hpp"

namespace oge::runtime::terrain
{

enum class TerrainObject
{
    Chunk,
    BuiltChunkMesh,
    MeshingWorkerContext,
};

using ChunkHandle = Handle<TerrainObject::Chunk>;
using BuiltMeshHandle = Handle<TerrainObject::BuiltChunkMesh>;
using MeshingWorkerContextHandle = Handle<TerrainObject::MeshingWorkerContext>;

struct LocalUpdateBlockCmd
{
    uint32_t value;
    LocalPoint3 coord;
    bool touchesBorder;
};

enum class ChunkState
{
    GeneratingTerrain,
    Persistent,
    PendingDestroy,
};

inline size_t GetBlockIndex(uint8_t x, uint8_t y, uint8_t z)
{
    return ((size_t)x << CHUNK_SHIFT_X) + ((size_t)y << CHUNK_SHIFT_Y) + ((size_t)z << CHUNK_SHIFT_Z);
}

struct ChunkData
{
    // 16384 bytes
    uint32_t data[CHUNK_SIZE_TOTAL] = {};
    Point3 Coords = {};
    ChunkState state = ChunkState::GeneratingTerrain;

   public:
    ChunkData(Point3 coords) { Coords = coords; }
    uint32_t GetBlock(uint8_t x, uint8_t y, uint8_t z) const;
    void SetBlock(uint8_t x, uint8_t y, uint8_t z, uint32_t value);
};

class ChunkDataCollection
{
   public:
    const ChunkData* Get(ChunkHandle chunk) const { return chunkData.Get(chunk); }
    ChunkData* Get(ChunkHandle chunk) { return chunkData.Get(chunk); }

    std::tuple<ChunkHandle, const ChunkData*> Get(Point3 coord) const;
    std::tuple<ChunkHandle, ChunkData*> Get(Point3 coord);

    ChunkHandle GetHandle(Point3 coord);

    ChunkHandle AllocateChunk(Point3 coord);
    void FreeChunk(Point3 coord);
    void FreeChunk(ChunkHandle handle);

   private:
    Pool<TerrainObject::Chunk, ChunkData> chunkData;
    std::unordered_map<Point3, ChunkHandle> coordToChunks;
};

struct PaletteCompressedChunk
{
    std::vector<uint32_t> palette;
    uint8_t data[CHUNK_SIZE_TOTAL];

    static PaletteCompressedChunk FromChunkData(const ChunkData& c);
    uint32_t Get(int x, int y, int z) const { return palette[data[GetBlockIndex(x, y, z)]]; }
};

// allocate chunk -> generate terrain queue -> build mesh queue -> built chunk
// meshes -> upload with streaming manager -> remove built chunk meshes ->
// resident chunk any state -> destroy chunk
struct TerrainData
{
    ChunkDataCollection chunks;
    std::queue<ChunkHandle> generateTerrainQueue;
    std::unordered_set<Point3> chunksToDestroy;
    std::unordered_map<ChunkHandle, std::vector<LocalUpdateBlockCmd>, HandleHash<ChunkHandle>> blockModificationQueue;
    std::unordered_set<ChunkHandle, HandleHash<ChunkHandle>> dirtyChunks;
};

struct ResolveDirtyChunkEvent
{
    ChunkHandle chunk;
};

struct TerrainRaycastResult
{
    uint8_t hitFace;
    Point3 hitPos;
    uint32_t hitBlockValue;
};

class TerrainService;

class TerrainView
{
    friend class SubsystemTerrain;
    friend class TerrainRenderer;

   public:
    uint32_t GetBlock(Point3 pos) const { return GetBlock(pos.x, pos.y, pos.z); }
    bool TryGetBlock(Point3 pos, uint32_t& value) const { return TryGetBlock(pos.x, pos.y, pos.z, value); }
    void SetBlock(Point3 pos, uint32_t value) { SetBlock(pos.x, pos.y, pos.z, value); }
    uint32_t GetBlock(int x, int y, int z) const;
    bool TryGetBlock(int x, int y, int z, uint32_t& value) const;
    void SetBlock(int x, int y, int z, uint32_t value);
    std::tuple<ChunkHandle, const ChunkData*> GetChunk(Point3 chunkCoord) const;
    std::tuple<ChunkHandle, ChunkData*> GetChunk(Point3 chunkCoord);
    const ChunkData* GetChunk(ChunkHandle handle) const;
    ChunkData* GetChunk(ChunkHandle handle);
    void SubmitChunk(ChunkHandle handle);
    std::optional<TerrainRaycastResult> CastRay(math::vec3 pos, math::vec3 ray, float maxDist = 20.f);

   protected:
    TerrainData m_terrainData;

   private:
    static void HandleResolveDirtyChunk(entt::registry& world, ResolveDirtyChunkEvent e);
};

}  // namespace oge::runtime::terrain
