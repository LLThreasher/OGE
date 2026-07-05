#pragma once

#include <unordered_map>
#include <unordered_set>
#include <queue>
#include "Engine/Point3.hpp"
#include "Engine/ResourcePool.hpp"
#include "Engine/Terrain/TerrainDefs.hpp"

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

    uint32_t GetBlock(uint8_t x, uint8_t y, uint8_t z) const
    {
        assert(0 <= x && x < CHUNK_SIZE_X);
        assert(0 <= y && y < CHUNK_SIZE_Y);
        assert(0 <= z && z < CHUNK_SIZE_Z);
        return data[GetBlockIndex(x, y, z)];
    }
    
    void SetBlock(uint8_t x, uint8_t y, uint8_t z, uint32_t value) { data[GetBlockIndex(x, y, z)] = value; }
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

    ChunkHandle GetHandle(Point3 coord)
    {
        auto it = coordToChunks.find(coord);
        if (it != coordToChunks.end())
        {
            return it->second;
        }
        return {};
    }

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
    std::unordered_map<Point3, ChunkHandle> coordToChunks;
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
    friend class TerrainService;
    friend class TerrainRenderer;
   public:
    uint32_t GetBlock(Point3 pos) const
    {
        return GetBlock(pos.x, pos.y, pos.z);
    }
    bool TryGetBlock(Point3 pos, uint32_t& value) const
    {
        return TryGetBlock(pos.x, pos.y, pos.z, value);
    }
    void SetBlock(Point3 pos, uint32_t value)
    {
        SetBlock(pos.x, pos.y, pos.z, value);
    }
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

}
