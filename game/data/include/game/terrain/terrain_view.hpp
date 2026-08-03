#pragma once

#include <queue>
#include <unordered_map>
#include <unordered_set>

#include "defs.hpp"
#include "entt/signal/fwd.hpp"
#include "oge/event_stream.hpp"
#include "oge/math.hpp"
#include "oge/point3.hpp"
#include "oge/pool.hpp"
#include "oge/runtime/entt.hpp"
#include "oge/runtime/typed_registry.hpp"

namespace game
{
namespace math = ::oge::math;
namespace sim::terrain
{
class SubsystemTerrain;
}  // namespace sim::terrain

namespace view::terrain
{
class TerrainRenderer;
}  // namespace view::terrain

namespace game::math
{
using namespace oge::math;
}

namespace terrain
{
using oge::Handle;
using oge::HandleHash;
using oge::LocalPoint3;
using oge::Point3;
using oge::Pool;

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

enum class ChunkState : uint8_t
{
    GeneratingTerrain = 0,
    InvalidLighting,
    Persistent,
    PendingDestroy,
};

constexpr bool operator<(ChunkState a, ChunkState b)
{
    return static_cast<uint8_t>(a) < static_cast<uint8_t>(b);
}

inline size_t GetBlockIndex(uint8_t x, uint8_t y, uint8_t z)
{
    return ((size_t)x << CHUNK_SHIFT_X) + ((size_t)y << CHUNK_SHIFT_Y) +
           ((size_t)z << CHUNK_SHIFT_Z);
}

struct ChunkData
{
    // 16384 bytes
    uint32_t data[CHUNK_SIZE_TOTAL] = {};
    Point3 Coords = {};
    ChunkState state = ChunkState::GeneratingTerrain;
    // this is only assigned to state if all neighbors have it
    ChunkState weakState = ChunkState::GeneratingTerrain;

   public:
    ChunkData(Point3 coords)
    {
        Coords = coords;
    }
    uint32_t GetBlock(uint8_t x, uint8_t y, uint8_t z) const;
    void SetBlock(uint8_t x, uint8_t y, uint8_t z, uint32_t value);
};

class ChunkDataCollection
{
   public:
    const ChunkData* Get(ChunkHandle chunk) const
    {
        return chunkData.Get(chunk);
    }
    ChunkData* Get(ChunkHandle chunk)
    {
        return chunkData.Get(chunk);
    }

    std::tuple<ChunkHandle, const ChunkData*> Get(Point3 coord) const;
    std::tuple<ChunkHandle, ChunkData*> Get(Point3 coord);

    ChunkHandle GetHandle(Point3 coord) const;

    ChunkHandle AllocateChunk(Point3 coord);
    void FreeChunk(Point3 coord);
    void FreeChunk(ChunkHandle handle);

    ChunkData* Poll(ChunkHandle& cursor);
    const ChunkData* Poll(ChunkHandle& cursor) const;

   private:
    Pool<TerrainObject::Chunk, ChunkData> chunkData;
    std::unordered_map<Point3, ChunkHandle> coordToChunks;
};

inline static uint64_t HashBytes(uint64_t hash, const void* data, size_t len)
{
    constexpr uint64_t FNV_offset = 14695981039346656037ull;
    constexpr uint64_t FNV_prime = 1099511628211ull;

    const uint8_t* bytes = static_cast<const uint8_t*>(data);

    if (hash == 0) hash = FNV_offset;

    for (size_t i = 0; i < len; ++i)
    {
        hash ^= bytes[i];
        hash *= FNV_prime;
    }

    return hash;
}

struct PaletteCompressedChunk
{
    std::pmr::vector<uint32_t> palette;
    uint8_t data[CHUNK_SIZE_TOTAL];  // 4096 bytes

    static void FromChunkData(const ChunkData& c, PaletteCompressedChunk& cc);
    void ToChunkData(ChunkData& c);
    uint32_t Get(int x, int y, int z) const
    {
        return palette[data[GetBlockIndex(x, y, z)]];
    }
};

inline uint64_t DebugHash(const PaletteCompressedChunk& chunk)
{
    uint64_t hash = 0;

    // Hash palette size
    uint32_t paletteSize = static_cast<uint32_t>(chunk.palette.size());
    hash = HashBytes(hash, &paletteSize, sizeof(paletteSize));

    // Hash palette contents
    if (!chunk.palette.empty())
    {
        hash = HashBytes(hash, chunk.palette.data(),
                         chunk.palette.size() * sizeof(uint32_t));
    }

    // Hash block data (4096 bytes)
    hash = HashBytes(hash, chunk.data, CHUNK_SIZE_TOTAL);

    return hash;
}

// allocate chunk -> generate terrain queue -> build mesh queue -> built chunk
// meshes -> upload with streaming manager -> remove built chunk meshes ->
// resident chunk any state -> destroy chunk
struct TerrainData
{
    ChunkDataCollection chunks;
    std::queue<ChunkHandle> generateTerrainQueue;
    std::unordered_set<Point3> chunksToDestroy;
    std::unordered_map<ChunkHandle, std::vector<LocalUpdateBlockCmd>,
                       HandleHash<ChunkHandle>>
        blockModificationQueue;

    TerrainData()
    {
    }
    NO_COPY(TerrainData)
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

struct ChunkStateUpdateEvent
{
    ChunkState prevState;
    ChunkState state;
    ChunkHandle chunk;
    bool weak = false;
};

class ChunkEventStream
    : public oge::DiscreteEventStream<terrain::ChunkStateUpdateEvent, 128>
{
};

class TerrainView
{
    friend class ::game::sim::terrain::SubsystemTerrain;
    friend class ::game::view::terrain::TerrainRenderer;

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
    const ChunkData* PollChunk(ChunkHandle& handle) const;
    ChunkData* GetChunk(ChunkHandle handle);
    ChunkHandle CreateChunk(Point3 chunkCoord);
    void UpgradeChunk(ChunkHandle handle, ChunkState newState);
    void DowngradeChunk(ChunkHandle handle, ChunkState newState);
    std::optional<TerrainRaycastResult> CastRay(math::vec3 pos, math::vec3 ray,
                                                float maxDist = 20.f);
    const ChunkEventStream& GetEvents() const
    {
        return m_chunkEvents;
    }

   protected:
    TerrainData m_terrainData;
    ChunkEventStream m_chunkEvents;

    void UpgradeChunkInternal(ChunkHandle handle, ChunkState newState, bool updateNeighbors);
};

}  // namespace terrain
}  // namespace game

namespace oge::runtime
{
template <>
struct TypeName<::game::terrain::TerrainView>
{
    static constexpr std::string Get()
    {
        return "core::TerrainView";
    }
};
}  // namespace oge::runtime
