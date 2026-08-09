#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "game/input/net.hpp"
#include "game/input/player_input_stream.hpp"
#include "game/net/event_log_stream.hpp"
#include "game/terrain/defs.hpp"
#include "game/terrain/terrain_view.hpp"
#include "oge/point3.hpp"
#include "oge/runtime/entt.hpp"
#include "oge/runtime/net_serializer.hpp"
#include "oge/runtime/net_traits.hpp"

// =========================================================================
// NetTraits for entt::entity
// (entity is a distinct enum type, not directly a FixedWidthInteger)
// =========================================================================

namespace oge::runtime::net
{

template <>
struct NetTraits<entt::entity> : SimpleValueTraits<entt::entity>
{
};

// NetTraits for IntTriple<T> (covers Point3, LocalPoint3, LocalUPoint3)
template <FixedWidthInteger T>
struct NetTraits<oge::IntTriple<T>>
{
    static constexpr uint64_t Size(const oge::IntTriple<T>&)
    {
        return sizeof(T) * 3;
    }

    static void Serialize(Buffer& buffer, const oge::IntTriple<T>& value)
    {
        buffer.template Write<T>(value.x);
        buffer.template Write<T>(value.y);
        buffer.template Write<T>(value.z);
    }

    static void Deserialize(Buffer& buffer, oge::IntTriple<T>& value)
    {
        value.x = buffer.template Read<T>();
        value.y = buffer.template Read<T>();
        value.z = buffer.template Read<T>();
    }
};

}  // namespace oge::runtime::net

namespace game::net
{

namespace net = oge::runtime::net;
using oge::runtime::oge_id_type;

// =========================================================================
// Entity events
// =========================================================================

struct AddEntityEvent
{
    entt::entity entity = entt::null;
};

struct RemoveEntityEvent
{
    entt::entity entity = entt::null;
};

// =========================================================================
// Component events (templated)
// =========================================================================

template <typename T>
struct AddComponentEvent
{
    entt::entity entity = entt::null;
    T component{};
};

template <typename T>
struct RemoveComponentEvent
{
    entt::entity entity = entt::null;
};

template <typename T>
struct UpdateComponentEvent
{
    entt::entity entity = entt::null;
    T component{};
};

// =========================================================================
// Chunk events
// =========================================================================

struct ChunkBlockUpdate
{
    oge::CompactLocalPoint3 position{};
    uint32_t block = 0;
};

struct AddChunkEvent
{
    oge::Point3 coords{};
    std::pmr::vector<uint32_t> blocks;  // CHUNK_SIZE_TOTAL elements

    // When true, serialization will RLE-compress the block data.
    // When false, blocks are serialized as raw uint32_t values.
    bool compressed = false;
};

// ---------------------------------------------------------------------------
// RLE compression helpers
// ---------------------------------------------------------------------------

inline std::pmr::vector<uint8_t> CompressChunk(const uint8_t* data,
                                               size_t size)
{
    std::pmr::vector<uint8_t> out{};
    if (size == 0) return out;

    out.reserve(size);
    size_t i = 0;
    while (i < size)
    {
        uint8_t value = data[i];
        uint8_t count = 1;
        while (i + count < size && data[i + count] == value && count < 255)
        {
            ++count;
        }
        out.push_back(count);
        out.push_back(value);
        i += count;
    }
    return out;
}

inline void DecompressChunk(const uint8_t* compressed, size_t compressedSize,
                            uint8_t* out, size_t expectedSize)
{
    size_t inOffset = 0;
    size_t outOffset = 0;
    while (inOffset < compressedSize)
    {
        OGE_ASSERT(inOffset + 2 <= compressedSize,
                   "RLE decompression: unexpected end of data at offset {} / {}",
                   inOffset, compressedSize);
        uint8_t count = compressed[inOffset++];
        uint8_t value = compressed[inOffset++];
        OGE_ASSERT(outOffset + count <= expectedSize,
                   "RLE decompression: output overflow at offset {} + {} > {}",
                   outOffset, count, expectedSize);
        std::memset(out + outOffset, value, count);
        outOffset += count;
    }
    OGE_ASSERT(outOffset == expectedSize,
               "RLE decompression: size mismatch (got {}, expected {})",
               outOffset, expectedSize);
}

struct RemoveChunkEvent
{
    oge::Point3 coords{};
};

struct UpdateChunkEvent
{
    oge::Point3 coords{};
    uint8_t dirtyCnt = 0;
    std::array<ChunkBlockUpdate, 29> updates{};
};

// =========================================================================
// Player input event
// =========================================================================

struct PlayerInputReplicationEvent
{
    entt::entity playerEntity = entt::null;
    input::net::PackedPlayerInputFrame frame{};
};

// =========================================================================
// State structs emplaced in world.ctx() for hook bookkeeping
// =========================================================================

namespace terrain = ::game::terrain;

// Tracks per-player input stream cursors.
// Emplaced in world.ctx() by InstallPlayerInputReplicationHooks.
struct PlayerInputReplicationState
{
    // Cursor per player entity — each player has an independent input stream.
    std::unordered_map<entt::entity, input::PlayerInputStream::Cursor> cursors;

    // The PlayerInputStream per entity must be accessible.  If your project
    // stores streams elsewhere (e.g. a component), adjust PollPlayerInputs
    // to retrieve them accordingly.
    std::unordered_map<entt::entity, input::PlayerInputStream*> streams;
};

// Tracks the cursor into the terrain ChunkEventStream.
// Emplaced in world.ctx() by InstallTerrainReplicationHooks.
struct TerrainReplicationState
{
    terrain::ChunkEventStream::Cursor chunkEventCursor{};
    bool initialized = false;
};

// ---------------------------------------------------------------------------
// Accessor for the EventLogStream stored in world.ctx()
// ---------------------------------------------------------------------------

inline EventLogStream<>& GetReplicationStream(entt::registry& world)
{
    return world.ctx().get<EventLogStream<>>();
}

// ---------------------------------------------------------------------------
// Push helper — serialize event into the EventLogStream under its own type id
// ---------------------------------------------------------------------------

template <typename TEvent>
void PushReplicationEvent(entt::registry& world, const TEvent& event)
{
    auto& stream = GetReplicationStream(world);
    oge_id_type eventId = entt::type_hash<TEvent>::value();

    auto buf = stream.EnqueueEvent(eventId, net::Size(event));
    net::Serialize(buf, event);
}

// =========================================================================
// Entity hook installers (one per event type)
// =========================================================================

inline void InstallAddEntityHooks(EventLogStream<>&, entt::registry& world)
{
    world.on_construct<ReplicatedTag>()
        .template connect<
            +[](entt::registry& world, entt::entity entity)
            {
                PushReplicationEvent(world, AddEntityEvent{entity});
            }>();
}

inline void InstallRemoveEntityHooks(EventLogStream<>&, entt::registry& world)
{
    world.on_destroy<ReplicatedTag>()
        .template connect<
            +[](entt::registry& world, entt::entity entity)
            {
                PushReplicationEvent(world, RemoveEntityEvent{entity});
            }>();
}

// Convenience: install both entity hooks at once (used from server_scene)
inline void InstallEntityReplicationHooks(entt::registry& world)
{
    InstallAddEntityHooks(GetReplicationStream(world), world);
    InstallRemoveEntityHooks(GetReplicationStream(world), world);
}

// =========================================================================
// Component hook installers (one per event type)
// =========================================================================

template <typename T>
void InstallAddComponentHooks(EventLogStream<>&, entt::registry& world)
{
    // Component added to an already-replicated entity.
    world.template on_construct<T>()
        .template connect<
            +[](entt::registry& world, entt::entity entity)
            {
                if (!world.all_of<ReplicatedTag>(entity))
                {
                    return;
                }
                PushReplicationEvent(
                    world,
                    AddComponentEvent<T>{entity, world.template get<T>(entity)});
            }>();

    // ReplicatedTag added to an entity that already has T.
    world.on_construct<ReplicatedTag>()
        .template connect<
            +[](entt::registry& world, entt::entity entity)
            {
                if (!world.template all_of<T>(entity))
                {
                    return;
                }
                PushReplicationEvent(
                    world,
                    AddComponentEvent<T>{entity, world.template get<T>(entity)});
            }>();
}

template <typename T>
void InstallUpdateComponentHooks(EventLogStream<>&, entt::registry& world)
{
    world.template on_update<T>()
        .template connect<
            +[](entt::registry& world, entt::entity entity)
            {
                if (!world.all_of<ReplicatedTag>(entity))
                {
                    return;
                }
                PushReplicationEvent(
                    world,
                    UpdateComponentEvent<T>{entity, world.template get<T>(entity)});
            }>();
}

template <typename T>
void InstallRemoveComponentHooks(EventLogStream<>&, entt::registry& world)
{
    world.template on_destroy<T>()
        .template connect<
            +[](entt::registry& world, entt::entity entity)
            {
                if (!world.all_of<ReplicatedTag>(entity))
                {
                    return;
                }
                PushReplicationEvent(
                    world,
                    RemoveComponentEvent<T>{entity});
            }>();
}

// Convenience: install all component hooks for T at once (used from
// server_scene)
template <typename T>
void InstallComponentReplicationHooks(entt::registry& world)
{
    auto& stream = GetReplicationStream(world);
    InstallAddComponentHooks<T>(stream, world);
    InstallUpdateComponentHooks<T>(stream, world);
    InstallRemoveComponentHooks<T>(stream, world);
}

// =========================================================================
// Terrain chunk hook installers
//
// Each installer ensures the TerrainReplicationState is present in ctx.
// PollTerrainChunkEvents (called each tick) does the actual work.
// =========================================================================

inline void InstallAddChunkHooks(EventLogStream<>&, entt::registry& world)
{
    if (!world.ctx().contains<TerrainReplicationState>())
        world.ctx().emplace<TerrainReplicationState>();
    auto& state = world.ctx().get<TerrainReplicationState>();
    state.initialized = true;
}

inline void InstallRemoveChunkHooks(EventLogStream<>&, entt::registry& world)
{
    if (!world.ctx().contains<TerrainReplicationState>())
        world.ctx().emplace<TerrainReplicationState>();
    auto& state = world.ctx().get<TerrainReplicationState>();
    state.initialized = true;
}

inline void InstallUpdateChunkHooks(EventLogStream<>&, entt::registry& world)
{
    if (!world.ctx().contains<TerrainReplicationState>())
        world.ctx().emplace<TerrainReplicationState>();
    auto& state = world.ctx().get<TerrainReplicationState>();
    state.initialized = true;
}

inline void InstallTerrainReplicationHooks(entt::registry& world)
{
    auto& stream = GetReplicationStream(world);
    InstallAddChunkHooks(stream, world);
    InstallRemoveChunkHooks(stream, world);
    InstallUpdateChunkHooks(stream, world);
}

// Call this each tick (e.g. from a subsystem) to flush terrain events into
// the replication stream.
inline void PollTerrainChunkEvents(entt::registry& world)
{
    if (!world.ctx().contains<TerrainReplicationState>())
    {
        return;
    }

    auto& state = world.ctx().get<TerrainReplicationState>();
    if (!state.initialized)
    {
        return;
    }

    if (!world.ctx().contains<terrain::TerrainView>())
    {
        return;
    }

    auto& terrain = world.ctx().get<terrain::TerrainView>();
    const auto& events = terrain.GetEvents();

    terrain::ChunkStateUpdateEvent chunkEvent{};
    while (events.PollOne(state.chunkEventCursor, chunkEvent))
    {
        if (!chunkEvent.IsValid())
        {
            continue;
        }

        // Chunk became persistent → AddChunk
        if (chunkEvent.packed.prevState != terrain::ChunkState::Persistent &&
            chunkEvent.packed.state == terrain::ChunkState::Persistent)
        {
            const terrain::ChunkData* chunk =
                terrain.GetChunk(chunkEvent.chunk);
            if (chunk != nullptr)
            {
                AddChunkEvent evt{};
                evt.coords = chunk->Coords;
                evt.blocks.resize(terrain::CHUNK_SIZE_TOTAL);
                for (size_t i = 0; i < terrain::CHUNK_SIZE_TOTAL; ++i)
                {
                    evt.blocks[i] = chunk->data[i];
                }
                PushReplicationEvent(world, evt);
            }
        }

        // Chunk stopped being persistent → RemoveChunk
        if (chunkEvent.packed.prevState == terrain::ChunkState::Persistent &&
            chunkEvent.packed.state != terrain::ChunkState::Persistent)
        {
            const terrain::ChunkData* chunk =
                terrain.GetChunk(chunkEvent.chunk);
            if (chunk != nullptr)
            {
                RemoveChunkEvent evt{chunk->Coords};
                PushReplicationEvent(world, evt);
            }
        }

        // Persistent chunk with dirty blocks → UpdateChunk
        if (chunkEvent.packed.state == terrain::ChunkState::Persistent &&
            chunkEvent.packed.dirtyCnt > 0 &&
            chunkEvent.packed.dirtyCnt <= 29)
        {
            const terrain::ChunkData* chunk =
                terrain.GetChunk(chunkEvent.chunk);
            if (chunk != nullptr)
            {
                UpdateChunkEvent evt{};
                evt.coords = chunk->Coords;
                evt.dirtyCnt = chunkEvent.packed.dirtyCnt;
                for (uint8_t i = 0; i < chunkEvent.packed.dirtyCnt; ++i)
                {
                    const oge::LocalUPoint3 p = chunkEvent.dirtyBlocks[i];
                    evt.updates[i] = ChunkBlockUpdate{
                        chunkEvent.dirtyBlocks[i],
                        chunk->GetBlock(p)};
                }
                PushReplicationEvent(world, evt);
            }
        }
    }
}

// =========================================================================
// Player input replication hooks
//
// PlayerInputStream instances are stored per player entity.  Use
// RegisterPlayerInputStream / UnregisterPlayerInputStream to manage the
// bookkeeping, then call PollPlayerInputs each tick.
// =========================================================================

inline void InstallPlayerInputReplicationHooks(entt::registry& world)
{
    if (!world.ctx().contains<PlayerInputReplicationState>())
        world.ctx().emplace<PlayerInputReplicationState>();
}

// Register a player's input stream so it is polled each tick.
inline void RegisterPlayerInputStream(entt::registry& world,
                                      entt::entity player,
                                      input::PlayerInputStream* stream)
{
    if (!world.ctx().contains<PlayerInputReplicationState>())
        world.ctx().emplace<PlayerInputReplicationState>();
    auto& state = world.ctx().get<PlayerInputReplicationState>();

    state.streams[player] = stream;

    // Initialise the cursor to the current end of each sub-stream so we only
    // capture input produced from this point forward.
    input::PlayerInputStream::Cursor cursor{};
    stream->AdvanceCursor(cursor);
    state.cursors[player] = cursor;
}

// Stop polling a player's input stream.
inline void UnregisterPlayerInputStream(entt::registry& world,
                                        entt::entity player)
{
    if (!world.ctx().contains<PlayerInputReplicationState>())
    {
        return;
    }

    auto& state = world.ctx().get<PlayerInputReplicationState>();
    state.streams.erase(player);
    state.cursors.erase(player);
}

// Call this each tick to flush player input frames into the replication
// stream.  All input is sent through the reliable channel.
inline void PollPlayerInputs(entt::registry& world)
{
    if (!world.ctx().contains<PlayerInputReplicationState>())
    {
        return;
    }

    auto& state = world.ctx().get<PlayerInputReplicationState>();

    for (auto& [player, stream] : state.streams)
    {
        if (stream == nullptr)
        {
            continue;
        }

        auto& cursor = state.cursors[player];

        input::net::PlayerInputFrame frame{};

        // Collect actions.
        input::PlayerInputEvent action{};
        while (stream->PollAction(cursor, action))
        {
            frame.inputEvents.push_back(action);
        }

        // Collect move deltas.
        math::vec2 move{};
        while (stream->PollMoveDelta(cursor, move))
        {
            frame.moveDelta += move;
        }

        // Collect aim (latest-wins within the tick).
        math::vec2 aim{};
        bool hasAim = false;
        while (stream->PollAim(cursor, aim))
        {
            frame.panDelta = aim;
            hasAim = true;
        }

        // Only emit an event if there is something to send.
        if (frame.inputEvents.empty() && frame.moveDelta == math::vec2{} &&
            !hasAim)
        {
            continue;
        }

        input::net::PackedPlayerInputFrame packed{frame};
        PlayerInputReplicationEvent evt{player, packed};
        PushReplicationEvent(world, evt);
    }
}

// =========================================================================
// Player input apply function
// =========================================================================

inline void ApplyEvent(entt::registry& world,
                       const PlayerInputReplicationEvent& event)
{
    if (!world.ctx().contains<PlayerInputReplicationState>())
    {
        return;
    }

    auto& state = world.ctx().get<PlayerInputReplicationState>();

    auto it = state.streams.find(event.playerEntity);
    if (it == state.streams.end() || it->second == nullptr)
    {
        return;
    }

    input::PlayerInputStream* stream = it->second;

    // Unpack and insert.
    input::net::PlayerInputFrame frame = event.frame;

    for (const auto& action : frame.inputEvents)
    {
        stream->InsertAction(action);
    }

    if (frame.moveDelta.x != 0.0f || frame.moveDelta.y != 0.0f)
    {
        stream->InsertMoveDelta(frame.moveDelta);
    }

    if (frame.panDelta.x != 0.0f || frame.panDelta.y != 0.0f)
    {
        stream->SetAim(frame.panDelta);
    }
}

// =========================================================================
// Entity apply functions
// =========================================================================

inline void ApplyEvent(entt::registry& world, const AddEntityEvent& event)
{
    if (event.entity == entt::null)
    {
        return;
    }

    if (!world.valid(event.entity))
    {
        auto _ = world.create(event.entity);
        (void)_;
    }

    if (!world.all_of<ReplicatedTag>(event.entity))
    {
        world.emplace<ReplicatedTag>(event.entity);
    }
}

inline void ApplyEvent(entt::registry& world, const RemoveEntityEvent& event)
{
    if (event.entity == entt::null)
    {
        return;
    }

    if (world.valid(event.entity))
    {
        world.destroy(event.entity);
    }
}

// =========================================================================
// Component apply functions
// =========================================================================

template <typename T>
void ApplyEvent(entt::registry& world, const AddComponentEvent<T>& event)
{
    if (event.entity == entt::null)
    {
        return;
    }

    if (!world.valid(event.entity))
    {
        auto _ = world.create(event.entity);
        (void)_;
    }

    world.template emplace_or_replace<T>(event.entity, event.component);
}

template <typename T>
void ApplyEvent(entt::registry& world, const UpdateComponentEvent<T>& event)
{
    if (event.entity == entt::null)
    {
        return;
    }

    if (!world.valid(event.entity))
    {
        return;
    }

    world.template emplace_or_replace<T>(event.entity, event.component);
}

template <typename T>
void ApplyEvent(entt::registry& world, const RemoveComponentEvent<T>& event)
{
    if (event.entity == entt::null)
    {
        return;
    }

    if (world.valid(event.entity) &&
        world.template all_of<T>(event.entity))
    {
        world.template remove<T>(event.entity);
    }
}

// =========================================================================
// Chunk apply functions
// =========================================================================

inline void ApplyEvent(entt::registry& world, const AddChunkEvent& event)
{
    auto& terrain = world.ctx().get<terrain::TerrainView>();

    auto [handle, chunk] = terrain.GetChunk(event.coords);

    if (chunk == nullptr)
    {
        handle = terrain.CreateChunk(event.coords);
        chunk = terrain.GetChunk(handle);
    }

    if (chunk == nullptr)
    {
        return;
    }

    if (event.blocks.size() == terrain::CHUNK_SIZE_TOTAL)
    {
        for (size_t i = 0; i < terrain::CHUNK_SIZE_TOTAL; ++i)
        {
            chunk->data[i] = event.blocks[i];
        }
    }

    chunk->Coords = event.coords;

    terrain.DowngradeChunk(handle, terrain::ChunkState::InvalidLighting);
    terrain.UpgradeChunk(handle, terrain::ChunkState::Persistent);
}

inline void ApplyEvent(entt::registry& world, const RemoveChunkEvent& event)
{
    auto& terrain = world.ctx().get<terrain::TerrainView>();

    auto [handle, chunk] = terrain.GetChunk(event.coords);

    if (chunk == nullptr)
    {
        return;
    }

    terrain.DowngradeChunk(handle, terrain::ChunkState::InvalidLighting);
}

inline void ApplyEvent(entt::registry& world, const UpdateChunkEvent& event)
{
    auto& terrain = world.ctx().get<terrain::TerrainView>();

    auto [handle, chunk] = terrain.GetChunk(event.coords);

    if (chunk == nullptr ||
        chunk->state != terrain::ChunkState::Persistent)
    {
        return;
    }

    for (uint8_t i = 0; i < event.dirtyCnt; ++i)
    {
        const ChunkBlockUpdate& upd = event.updates[i];
        const oge::LocalUPoint3 p = upd.position;

        chunk->SetBlock(p, upd.block);
    }

    terrain.DowngradeChunk(handle, terrain::ChunkState::InvalidLighting);
    terrain.UpgradeChunk(handle, terrain::ChunkState::Persistent);
}

}  // namespace game::net

// =========================================================================
// TypeName specializations for event types (needed by TypeRegistry)
// =========================================================================

// Entity events
DECL_TYPE_NAME(game::net::AddEntityEvent, "net::AddEntityEvent")
DECL_TYPE_NAME(game::net::RemoveEntityEvent, "net::RemoveEntityEvent")

// Chunk events
DECL_TYPE_NAME(game::net::AddChunkEvent, "net::AddChunkEvent")
DECL_TYPE_NAME(game::net::RemoveChunkEvent, "net::RemoveChunkEvent")
DECL_TYPE_NAME(game::net::UpdateChunkEvent, "net::UpdateChunkEvent")
DECL_TYPE_NAME(game::net::PlayerInputReplicationEvent,
               "net::PlayerInputReplicationEvent")

namespace oge::runtime
{

// Generic TypeName for component event templates — generates a name
// from the inner component type.
template <typename T>
struct TypeName<game::net::AddComponentEvent<T>>
{
    static std::string Get()
    {
        return TypeName<T>::Get() + ".AddComponentEvent";
    }
};

template <typename T>
struct TypeName<game::net::UpdateComponentEvent<T>>
{
    static std::string Get()
    {
        return TypeName<T>::Get() + ".UpdateComponentEvent";
    }
};

template <typename T>
struct TypeName<game::net::RemoveComponentEvent<T>>
{
    static std::string Get()
    {
        return TypeName<T>::Get() + ".RemoveComponentEvent";
    }
};

}  // namespace oge::runtime

// =========================================================================
// NetTraits for entity events
// =========================================================================

DECL_NET_OBJ(game::net::AddEntityEvent, {
    visit(self.entity);
})

DECL_NET_OBJ(game::net::RemoveEntityEvent, {
    visit(self.entity);
})

// =========================================================================
// NetTraits for component events (generic partial specializations)
// =========================================================================

namespace oge::runtime::net
{

template <typename T>
struct NetTraits<game::net::AddComponentEvent<T>>
    : ObjectTraits<game::net::AddComponentEvent<T>>
{
    template <typename F>
    static void VisitFields(game::net::AddComponentEvent<T>& self, F&& visit)
    {
        visit(self.entity);
        visit(self.component);
    }

    template <typename F>
    static void VisitFields(const game::net::AddComponentEvent<T>& self,
                            F&& visit)
    {
        visit(self.entity);
        visit(self.component);
    }
};

template <typename T>
struct NetTraits<game::net::RemoveComponentEvent<T>>
    : ObjectTraits<game::net::RemoveComponentEvent<T>>
{
    template <typename F>
    static void VisitFields(game::net::RemoveComponentEvent<T>& self,
                            F&& visit)
    {
        visit(self.entity);
    }

    template <typename F>
    static void VisitFields(const game::net::RemoveComponentEvent<T>& self,
                            F&& visit)
    {
        visit(self.entity);
    }
};

template <typename T>
struct NetTraits<game::net::UpdateComponentEvent<T>>
    : ObjectTraits<game::net::UpdateComponentEvent<T>>
{
    template <typename F>
    static void VisitFields(game::net::UpdateComponentEvent<T>& self,
                            F&& visit)
    {
        visit(self.entity);
        visit(self.component);
    }

    template <typename F>
    static void VisitFields(const game::net::UpdateComponentEvent<T>& self,
                            F&& visit)
    {
        visit(self.entity);
        visit(self.component);
    }
};

}  // namespace oge::runtime::net

// AddChunkEvent has custom serialization to support both compressed and
// non-compressed paths.  Must be in oge::runtime::net.
namespace oge::runtime::net
{

template <>
struct NetTraits<::game::net::AddChunkEvent>
{
    static uint64_t Size(const game::net::AddChunkEvent& value)
    {
        uint64_t sz = net::Size(value.coords) + sizeof(uint8_t);
        if (value.compressed && value.blocks.size() == ::game::terrain::CHUNK_SIZE_TOTAL)
        {
            auto rle = game::net::CompressChunk(
                reinterpret_cast<const uint8_t*>(value.blocks.data()),
                value.blocks.size() * sizeof(uint32_t));
            sz += sizeof(uint32_t) + rle.size();
        }
        else
        {
            sz += net::Size(value.blocks);
        }
        return sz;
    }

    static void Serialize(Buffer& buffer,
                          const game::net::AddChunkEvent& value)
    {
        net::Serialize(buffer, value.coords);
        uint8_t flags = value.compressed ? 1 : 0;
        buffer.Write(flags);

        if (value.compressed &&
            value.blocks.size() == ::game::terrain::CHUNK_SIZE_TOTAL)
        {
            auto rle = game::net::CompressChunk(
                reinterpret_cast<const uint8_t*>(value.blocks.data()),
                value.blocks.size() * sizeof(uint32_t));
            uint32_t rleSize = static_cast<uint32_t>(rle.size());
            buffer.Write(rleSize);
            buffer.WriteRaw(rle.data(), rle.size());
        }
        else
        {
            net::Serialize(buffer, value.blocks);
        }
    }

    static void Deserialize(Buffer& buffer,
                            game::net::AddChunkEvent& value)
    {
        value = {};
        net::Deserialize(buffer, value.coords);
        uint8_t flags = buffer.Read<uint8_t>();
        value.compressed = (flags & 1) != 0;

        if (value.compressed)
        {
            uint32_t rleSize = buffer.Read<uint32_t>();
            std::pmr::vector<uint8_t> compressed(rleSize);
            buffer.ReadRaw(compressed.data(), rleSize);

            size_t rawSize =
                ::game::terrain::CHUNK_SIZE_TOTAL * sizeof(uint32_t);
            value.blocks.resize(::game::terrain::CHUNK_SIZE_TOTAL);
            game::net::DecompressChunk(
                compressed.data(), rleSize,
                reinterpret_cast<uint8_t*>(value.blocks.data()), rawSize);
        }
        else
        {
            net::Deserialize(buffer, value.blocks);
        }
    }
};

}  // namespace oge::runtime::net

DECL_NET_OBJ(game::net::ChunkBlockUpdate, {
    visit(self.position.val);
    visit(self.block);
})

DECL_NET_OBJ(game::net::RemoveChunkEvent, {
    visit(self.coords);
})

DECL_NET_OBJ(game::net::UpdateChunkEvent, {
    visit(self.coords);
    visit(self.dirtyCnt);

    for (uint8_t i = 0; i < self.dirtyCnt && i < self.updates.size(); ++i)
    {
        visit(self.updates[i]);
    }
})

// =========================================================================
// NetTraits for player input event
// =========================================================================

DECL_NET_OBJ(game::net::PlayerInputReplicationEvent, {
    visit(self.playerEntity);
    visit(self.frame);
})
