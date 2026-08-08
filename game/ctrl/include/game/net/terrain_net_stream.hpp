#pragma once

#include "game/net/replication_registry.hpp"
#include "game/terrain/terrain_view.hpp"
#include "oge/point3.hpp"
#include "oge/runtime/net_serializer.hpp"
#include "oge/runtime/net_traits.hpp"

namespace game::net
{

enum class ChunkPacketType : uint8_t
{
    Load,
    LoadCompressed,
    Update,
    Discard,
};

struct TerrainChunkBlockUpdate
{
    oge::CompactLocalPoint3 position{};
    uint32_t block = 0;
};

struct TerrainChunkPacket
{
    ChunkPacketType type = ChunkPacketType::Load;

    oge::Point3 coords{};

    // Used by Load / LoadCompressed.
    // Serialization decides whether this becomes raw or compressed.
    const terrain::ChunkData* chunk = nullptr;

    // Used by Update.
    uint8_t dirtyCnt = 0;
    std::array<TerrainChunkBlockUpdate, 29> dirtyBlocks{};

    // Used in decode
    terrain::ChunkData decodedStorage{};

    bool IsFullChunk() const
    {
        return type == ChunkPacketType::Load ||
               type == ChunkPacketType::LoadCompressed;
    }

    bool IsUpdate() const
    {
        return type == ChunkPacketType::Update;
    }

    bool IsDiscard() const
    {
        return type == ChunkPacketType::Discard;
    }
};

class TerrainChunkNetOutStream
{
   public:
    using Packet = TerrainChunkPacket;
    using Event = Packet;

   private:
    struct TickCursorState
    {
        ReplicationTick tick = 0;

        /*
            Event cursor representing the terrain event stream position for this
            sender tick.
        */
        terrain::ChunkEventStream::Cursor eventCursor{};

        /*
            Network cursor for events belonging to this tick.

            Because NetCursor is uint32_t, this is a stream-local sequence
            number, not a terrain object-pool handle.
        */
        NetCursor eventNetCursor{};
    };

    struct SnapshotState
    {
        bool active = false;

        /*
            The tick this snapshot belongs to.

            A full terrain snapshot may take many sender ticks to transmit, but
            every packet from that snapshot carries this original tick.
        */
        ReplicationTick tick = 0;

        /*
            Internal terrain iterator.

            This is a generational object-pool handle and must not be encoded or
            converted into NetCursor.
        */
        terrain::ChunkHandle chunkCursor{};

        /*
            Network cursor for the snapshot stream.

            This is independent of terrain::ChunkHandle. It increments only when
            a sendable snapshot chunk is emitted.
        */
        NetCursor netCursor{};

        /*
            The event cursor captured when the snapshot began.

            Once snapshot streaming completes, incremental event sending resumes
            from this cursor so events that happened during the multi-tick
            snapshot are preserved.
        */
        terrain::ChunkEventStream::Cursor eventBeginCursor{};
    };

    struct EncodeState
    {
        SnapshotState snapshot{};

        /*
            Cursor used for incremental terrain events.

            This advances as events are consumed by this per-peer stream.
        */
        terrain::ChunkEventStream::Cursor chunkEventCursor{};

        /*
            Network cursor for incremental events.

            This is not derived from terrain::ChunkEventStream::Cursor. It is a
            stream-local uint32_t sequence.
        */
        NetCursor eventNetCursor{};

        /*
            Relationship between sender ticks and terrain event cursors.

            AdvanceTick records where the stream was when each sender tick was
            advanced.
        */
        std::deque<TickCursorState> tickCursors{};

        bool initialized = false;
    };

    entt::registry& m_registry;

    /*
        Per-peer sender stream state.

        Because this stream instance is per-peer on the sender, encode state can
        live directly on the stream.
    */
    mutable EncodeState m_encode{};

    /*
        Maximum number of logical terrain chunk packets this stream may produce
        per sender frame/tick.

        0 means unlimited.

        This is intentionally not tied to AdvanceTick(). Poll/Peek account for
        this limit based on EncodeContext::senderTick, so the replication layer
        may call AdvanceTick only when it actually advances its own tick.
    */
    size_t m_maxChunksPerFrame = 0;

    /*
        Per-frame transmit budget tracking.

        Mutable because Peek/Poll are const for the output-stream concept.
        Peek uses a copied EncodeState but may need to observe this budget.
        Poll consumes it.
    */
    mutable ReplicationTick m_transmitFrameTick = 0;
    mutable size_t m_transmittedThisFrame = 0;
    mutable bool m_transmitFrameInitialized = false;

   public:
    explicit TerrainChunkNetOutStream(entt::registry& registry)
        : m_registry(registry)
    {
    }

    explicit TerrainChunkNetOutStream(
        entt::registry& registry,
        size_t maxChunksPerFrame)
        : m_registry(registry)
        , m_maxChunksPerFrame(maxChunksPerFrame)
    {
    }

    void SetMaxChunksPerFrame(size_t maxChunksPerFrame)
    {
        m_maxChunksPerFrame = maxChunksPerFrame;
    }

    size_t GetMaxChunksPerFrame() const
    {
        return m_maxChunksPerFrame;
    }

    PacketPlan Peek(const EncodeContext& ectx) const
    {
        if (!HasFrameBudget(ectx.senderTick))
        {
            return {};
        }

        auto& terrain =
            const_cast<terrain::TerrainView&>(
                m_registry.ctx().get<terrain::TerrainView>());

        EncodeState copy = m_encode;
        EnsureInitialized(terrain, copy, ectx.senderTick);

        PlannedEvent planned{};
        if (!PeekNextFromState(terrain, copy, ectx, planned))
        {
            return {};
        }

        const size_t byteCount = EstimatePacketBytes(planned.packet);
        if (ectx.maxPacketBytes != 0 && byteCount > ectx.maxPacketBytes)
        {
            return {};
        }

        PacketPlan plan{};
        plan.hasPacket = true;
        plan.tick = planned.tick;
        plan.begin = planned.begin;
        plan.end = planned.end;
        plan.itemCount = 1;
        plan.byteCount = byteCount;

        return plan;
    }

    bool Poll(const EncodeContext& ectx, Packet& packet) const
    {
        if (!HasFrameBudget(ectx.senderTick))
        {
            return false;
        }

        auto& terrain = m_registry.ctx().get<terrain::TerrainView>();

        EnsureInitialized(terrain, m_encode, ectx.senderTick);

        PlannedEvent planned{};
        if (!PollNextFromState(terrain, m_encode, ectx, planned))
        {
            return false;
        }

        /*
            Registry should copy PacketPlan::tick into
            EncodeContext::packetTick before calling Poll.
        */
        if (planned.tick != ectx.packetTick)
        {
            return false;
        }

        if (planned.begin != ectx.begin)
        {
            return false;
        }

        if (ectx.itemCount != 0 && ectx.itemCount != 1)
        {
            return false;
        }

        const size_t byteCount = EstimatePacketBytes(planned.packet);
        if (ectx.maxPacketBytes != 0 && byteCount > ectx.maxPacketBytes)
        {
            return false;
        }

        packet = planned.packet;
        ConsumeFrameBudget(ectx.senderTick);
        return true;
    }

    void AdvanceTick(const AdvanceTickContext& tctx) const
    {
        auto& terrain = m_registry.ctx().get<terrain::TerrainView>();

        EnsureInitialized(terrain, m_encode, tctx.tick);

        /*
            Do not always append a tick cursor.

            The snapshot stream can span many replication ticks while still
            carrying the original snapshot tick in packet headers. During that
            phase, incremental event cursors are not being consumed, so appending
            a new identical tick cursor every AdvanceTick call is not useful.

            Once the snapshot is complete, record only actual sender-tick
            boundaries for the incremental event stream.
        */
        if (m_encode.snapshot.active)
        {
            return;
        }

        if (!m_encode.tickCursors.empty() &&
            m_encode.tickCursors.back().tick == tctx.tick)
        {
            return;
        }

        TickCursorState tickState{};
        tickState.tick = tctx.tick;
        tickState.eventCursor = m_encode.chunkEventCursor;
        tickState.eventNetCursor = m_encode.eventNetCursor;

        m_encode.tickCursors.push_back(tickState);

        constexpr size_t MaxTickCursorHistory = 256;
        while (m_encode.tickCursors.size() > MaxTickCursorHistory)
        {
            m_encode.tickCursors.pop_front();
        }
    }

   private:
    struct PlannedEvent
    {
        ReplicationTick tick = 0;
        NetCursor begin{};
        NetCursor end{};
        Packet packet{};
    };

    bool HasFrameBudget(ReplicationTick senderTick) const
    {
        ResetFrameBudgetIfNeeded(senderTick);

        return m_maxChunksPerFrame == 0 ||
               m_transmittedThisFrame < m_maxChunksPerFrame;
    }

    void ConsumeFrameBudget(ReplicationTick senderTick) const
    {
        ResetFrameBudgetIfNeeded(senderTick);

        if (m_maxChunksPerFrame != 0)
        {
            ++m_transmittedThisFrame;
        }
    }

    void ResetFrameBudgetIfNeeded(ReplicationTick senderTick) const
    {
        if (!m_transmitFrameInitialized ||
            m_transmitFrameTick != senderTick)
        {
            m_transmitFrameInitialized = true;
            m_transmitFrameTick = senderTick;
            m_transmittedThisFrame = 0;
        }
    }

    static bool IsSendableChunk(const terrain::ChunkData* chunk)
    {
        return chunk != nullptr &&
               chunk->state == terrain::ChunkState::Persistent;
    }

    static NetCursor AdvanceNetCursor(NetCursor cursor)
    {
        /*
            NetCursor is uint32_t. Wrapping is okay only if your receiver/window
            logic understands modular comparison. Otherwise assert before wrap.
        */
        return NetCursor{static_cast<uint32_t>(++cursor)};
    }

    static size_t EstimatePacketBytes(const Packet& packet)
    {
        /*
            Replace with your actual serialized-size function if available.

            This must match what encode writes closely enough to keep packets
            under maxPacketBytes.

            LoadCompressed is estimated like Load here because compression is
            controlled by serialization in the provided packet definition.
        */
        switch (packet.type)
        {
            case ChunkPacketType::Load:
            case ChunkPacketType::LoadCompressed:
                return sizeof(Packet);

            case ChunkPacketType::Update:
                return sizeof(packet.coords) +
                       sizeof(packet.type) +
                       sizeof(packet.dirtyCnt) +
                       packet.dirtyCnt * sizeof(packet.dirtyBlocks[0]);

            case ChunkPacketType::Discard:
                return sizeof(packet.coords) + sizeof(packet.type);

            default:
                return sizeof(Packet);
        }
    }

    static void EnsureInitialized(
        terrain::TerrainView& terrain,
        EncodeState& state,
        ReplicationTick senderTick)
    {
        if (state.initialized)
        {
            return;
        }

        state.initialized = true;

        /*
            Start this peer with a full snapshot.
        */
        state.snapshot.active = true;
        state.snapshot.tick = senderTick;
        state.snapshot.chunkCursor = terrain::ChunkHandle{};
        state.snapshot.netCursor = NetCursor{0};

        /*
            Capture the terrain event cursor at snapshot start. Incremental
            events resume from here after the snapshot finishes.
        */
        terrain.GetEvents().AdvanceCursor(state.snapshot.eventBeginCursor);

        state.chunkEventCursor = state.snapshot.eventBeginCursor;
        state.eventNetCursor = NetCursor{0};

        TickCursorState tickState{};
        tickState.tick = senderTick;
        tickState.eventCursor = state.chunkEventCursor;
        tickState.eventNetCursor = state.eventNetCursor;
        state.tickCursors.push_back(tickState);
    }

    static bool PeekNextFromState(
        terrain::TerrainView& terrain,
        EncodeState& state,
        const EncodeContext& ectx,
        PlannedEvent& planned)
    {
        /*
            Peek operates on a copied EncodeState, so it can reuse the mutating
            Poll implementation safely.
        */
        return PollNextFromState(terrain, state, ectx, planned);
    }

    static bool PollNextFromState(
        terrain::TerrainView& terrain,
        EncodeState& state,
        const EncodeContext& ectx,
        PlannedEvent& planned)
    {
        planned = {};

        if (state.snapshot.active)
        {
            if (PollNextSnapshotChunk(terrain, state, ectx, planned))
            {
                return true;
            }

            /*
                Snapshot completed.

                Resume incremental events from the event cursor captured when
                the snapshot began. This keeps modifications that occurred while
                the snapshot was streaming.
            */
            state.snapshot.active = false;
            state.chunkEventCursor = state.snapshot.eventBeginCursor;
            state.eventNetCursor = NetCursor{0};
        }

        return PollNextIncrementalEvent(terrain, state, ectx, planned);
    }

    static bool PollNextSnapshotChunk(
        terrain::TerrainView& terrain,
        EncodeState& state,
        const EncodeContext& ectx,
        PlannedEvent& planned)
    {
        (void)ectx;

        while (const terrain::ChunkData* chunk =
                   terrain.PollChunk(state.snapshot.chunkCursor))
        {
            if (!IsSendableChunk(chunk))
            {
                continue;
            }

            Packet packet{};
            packet.type = ChunkPacketType::Load;
            packet.coords = chunk->Coords;
            packet.chunk = chunk;
            packet.dirtyCnt = 0;
            packet.dirtyBlocks = {};

            const NetCursor begin = state.snapshot.netCursor;
            const NetCursor end = AdvanceNetCursor(begin);

            /*
                Only advance snapshot NetCursor when a sendable chunk is
                actually emitted. This avoids coupling cursor values to object
                pool slots or generational handles.
            */
            state.snapshot.netCursor = end;

            planned.tick = state.snapshot.tick;
            planned.begin = begin;
            planned.end = end;
            planned.packet = packet;

            return true;
        }

        return false;
    }

    static bool PollNextIncrementalEvent(
        terrain::TerrainView& terrain,
        EncodeState& state,
        const EncodeContext& ectx,
        PlannedEvent& planned)
    {
        terrain::ChunkStateUpdateEvent e{};

        while (terrain.GetEvents().PollOne(state.chunkEventCursor, e))
        {
            Packet packet{};

            if (!BuildPacketFromChunkEvent(terrain, e, packet))
            {
                continue;
            }

            const NetCursor begin = state.eventNetCursor;
            const NetCursor end = AdvanceNetCursor(begin);

            state.eventNetCursor = end;

            /*
                Incremental packets use the sender tick currently being
                produced.

                The stream records the event cursor <-> tick relationship in
                AdvanceTick. If terrain events later carry their own tick, use
                that event tick here instead.
            */
            planned.tick = ectx.senderTick;
            planned.begin = begin;
            planned.end = end;
            planned.packet = packet;

            return true;
        }

        return false;
    }

    static bool BuildPacketFromChunkEvent(
        terrain::TerrainView& terrain,
        const terrain::ChunkStateUpdateEvent& e,
        Packet& packet)
    {
        packet = {};

        if (!e.IsValid())
        {
            return false;
        }

        /*
            Discard packets do not require current chunk data.
        */
        if (e.packed.state != terrain::ChunkState::Persistent)
        {
            terrain::ChunkData* oldChunk = terrain.GetChunk(e.chunk);
            if (oldChunk != nullptr)
            {
                packet.type = ChunkPacketType::Discard;
                packet.coords = oldChunk->Coords;
                packet.chunk = nullptr;
                packet.dirtyCnt = 0;
                packet.dirtyBlocks = {};
                return true;
            }

            return false;
        }

        terrain::ChunkData* chunk = terrain.GetChunk(e.chunk);

        /*
            Only persistent chunks may be sent as Load/Update.

            This intentionally checks current chunk state. Since this is
            latest-basis replication, the current chunk is authoritative.
        */
        if (chunk == nullptr ||
            chunk->state != terrain::ChunkState::Persistent)
        {
            return false;
        }

        packet.coords = chunk->Coords;
        packet.chunk = chunk;

        if (e.IsAllDirty())
        {
            packet.type = ChunkPacketType::Load;
            packet.dirtyCnt = 0;
            packet.dirtyBlocks = {};
            return true;
        }

        if (e.packed.dirtyCnt > 0)
        {
            packet.type = ChunkPacketType::Update;
            packet.dirtyCnt = e.packed.dirtyCnt;

            for (uint8_t i = 0; i < e.packed.dirtyCnt; ++i)
            {
                const oge::LocalUPoint3 blk = e.dirtyBlocks[i];

                packet.dirtyBlocks[i] =
                    TerrainChunkBlockUpdate{
                        e.dirtyBlocks[i],
                        chunk->GetBlock(blk)};
            }

            return true;
        }

        /*
            State became persistent, but no dirty blocks were specified. Treat
            this as a full load because the remote may not have the chunk.
        */
        if (e.packed.prevState != terrain::ChunkState::Persistent &&
            e.packed.state == terrain::ChunkState::Persistent)
        {
            packet.type = ChunkPacketType::Load;
            packet.dirtyCnt = 0;
            packet.dirtyBlocks = {};
            return true;
        }

        return false;
    }
};

static_assert(IsNetOutputStream<TerrainChunkNetOutStream>);

class TerrainChunkNetInStream
{
   public:
    using Packet = TerrainChunkPacket;
    using Event = Packet;

   private:
    entt::registry& m_registry;

    Packet m_pending{};
    bool m_hasPending = false;

   public:
    explicit TerrainChunkNetInStream(entt::registry& registry)
        : m_registry(registry)
    {
    }

    bool Insert(const DecodeContext& dctx, const Packet& packet)
    {
        (void)dctx;

        if (m_hasPending)
        {
            return false;
        }

        if (!StorePending(packet))
        {
            return false;
        }

        m_hasPending = true;
        return true;
    }

    /*
        Apply is intentionally separate from Insert.

        It is not part of IsNetInputStream, but lets the replication layer defer
        terrain mutation until its receive/apply phase.
    */
    bool Apply()
    {
        if (!m_hasPending)
        {
            return false;
        }

        const bool result = ApplyPending(m_pending);

        m_pending = {};
        m_hasPending = false;

        return result;
    }

    bool Apply(const DecodeContext& dctx)
    {
        (void)dctx;
        return Apply();
    }

    void AdvanceTick(const AdvanceTickContext& tctx)
    {
        (void)tctx;
    }

   private:
    bool StorePending(const Packet& packet)
    {
        m_pending = packet;

        /*
            Full chunk packets may arrive in either form:
              - packet.chunk points at externally decoded storage
              - packet.chunk is null and packet.decodedStorage owns the decode

            Normalize so ApplyPending always reads from
            m_pending.decodedStorage.
        */
        if (m_pending.IsFullChunk())
        {
            const terrain::ChunkData* src = packet.chunk;

            if (src == nullptr)
            {
                src = &packet.decodedStorage;
            }

            if (src == nullptr)
            {
                return false;
            }

            m_pending.decodedStorage = *src;
            m_pending.decodedStorage.Coords = packet.coords;
            m_pending.chunk = &m_pending.decodedStorage;
            m_pending.dirtyCnt = 0;
            m_pending.dirtyBlocks = {};
            return true;
        }

        if (m_pending.IsUpdate())
        {
            m_pending.chunk = nullptr;

            if (m_pending.dirtyCnt > m_pending.dirtyBlocks.size())
            {
                return false;
            }

            return true;
        }

        if (m_pending.IsDiscard())
        {
            m_pending.chunk = nullptr;
            m_pending.dirtyCnt = 0;
            m_pending.dirtyBlocks = {};
            return true;
        }

        return false;
    }

    bool ApplyPending(const Packet& packet)
    {
        auto& terrain = m_registry.ctx().get<terrain::TerrainView>();

        if (packet.IsDiscard())
        {
            return ApplyDiscard(terrain, packet);
        }

        auto [handle, chunk] = terrain.GetChunk(packet.coords);

        if (chunk == nullptr)
        {
            handle = terrain.CreateChunk(packet.coords);
            chunk = terrain.GetChunk(handle);
        }

        if (chunk == nullptr)
        {
            return false;
        }

        if (packet.IsFullChunk())
        {
            return ApplyFullChunk(terrain, handle, chunk, packet);
        }

        if (packet.IsUpdate())
        {
            return ApplyUpdate(terrain, handle, chunk, packet);
        }

        return false;
    }

    static bool ApplyFullChunk(
        terrain::TerrainView& terrain,
        terrain::ChunkHandle handle,
        terrain::ChunkData* chunk,
        const Packet& packet)
    {
        if (packet.chunk == nullptr)
        {
            return false;
        }

        *chunk = *packet.chunk;
        chunk->Coords = packet.coords;

        terrain.DowngradeChunk(
            handle,
            terrain::ChunkState::InvalidLighting);

        terrain.UpgradeChunk(
            handle,
            terrain::ChunkState::Persistent);

        return true;
    }

    static bool ApplyUpdate(
        terrain::TerrainView& terrain,
        terrain::ChunkHandle handle,
        terrain::ChunkData* chunk,
        const Packet& packet)
    {
        /*
            If the remote does not have the full chunk yet, an update is not
            useful. Ignore it or request a resend/load elsewhere.
        */
        if (chunk->state != terrain::ChunkState::Persistent)
        {
            return false;
        }

        for (uint8_t i = 0; i < packet.dirtyCnt; ++i)
        {
            const TerrainChunkBlockUpdate& dirty = packet.dirtyBlocks[i];
            const oge::LocalUPoint3 p = dirty.position;

            chunk->SetBlock(p, dirty.block);
        }

        terrain.DowngradeChunk(
            handle,
            terrain::ChunkState::InvalidLighting);

        terrain.UpgradeChunk(
            handle,
            terrain::ChunkState::Persistent);

        return true;
    }

    static bool ApplyDiscard(
        terrain::TerrainView& terrain,
        const Packet& packet)
    {
        auto [handle, chunk] = terrain.GetChunk(packet.coords);

        if (chunk == nullptr)
        {
            return true;
        }

        /*
            Use your actual removal/unload API here if TerrainView exposes one.

            If the local terrain keeps chunk objects around but marks them
            non-persistent, this mirrors that behavior.
        */
        terrain.DowngradeChunk(
            handle,
            terrain::ChunkState::InvalidLighting);

        return true;
    }
};

static_assert(IsNetInputStream<TerrainChunkNetInStream>);

static std::pmr::vector<uint8_t> CompressChunk(const std::uint8_t* data,
                                               std::size_t size)
{
    std::pmr::vector<uint8_t> out{};

    if (size == 0) return out;

    out.reserve(size);

    std::size_t i = 0;

    while (i < size)
    {
        std::uint8_t value = data[i];
        std::uint8_t count = 1;

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

static void DecompressChunk(const std::uint8_t* compressed,
                            std::size_t compressedSize, std::uint8_t* out,
                            std::size_t expectedSize)
{
    std::size_t inOffset = 0;
    std::size_t outOffset = 0;

    while (inOffset < compressedSize)
    {
        if (inOffset + 2 > compressedSize)
        {
            assert(false && "Invalid RLE data");
        }

        std::uint8_t count = compressed[inOffset++];
        std::uint8_t value = compressed[inOffset++];

        if (outOffset + count > expectedSize)
        {
            assert(false && "RLE decompressed data too large");
        }

        std::memset(out + outOffset, value, count);
        outOffset += count;
    }

    if (outOffset != expectedSize)
    {
        assert(false && "RLE decompressed size mismatch");
    }
}
}

namespace oge::runtime::net
{
using game::net::TerrainChunkBlockUpdate;
using game::net::TerrainChunkPacket;

template <>
struct NetTraits<TerrainChunkBlockUpdate>
{
static void Serialize(Buffer& writer, const TerrainChunkBlockUpdate& update)
{
    writer.Write(update.position);
    writer.Write(update.block);
}
static void Deserialize(Buffer& reader, TerrainChunkBlockUpdate& update)
{
    reader.Read(update.position);
    reader.Read(update.block);
}
};

template <>
struct NetTraits<TerrainChunkPacket>
{
static void Serialize(Buffer& writer, const TerrainChunkPacket& packet)
{
    using namespace game::terrain;
    using namespace game::net;

    switch (packet.type)
    {
        case ChunkPacketType::Load:
        {
            assert(packet.chunk != nullptr);

            PaletteCompressedChunk cChunk;
            PaletteCompressedChunk::FromChunkData(*packet.chunk, cChunk);

            auto compressedData =
                CompressChunk(
                    reinterpret_cast<const std::uint8_t*>(cChunk.data),
                    CHUNK_SIZE_TOTAL);

            const ChunkPacketType wireType =
                compressedData.size() < CHUNK_SIZE_TOTAL
                    ? ChunkPacketType::LoadCompressed
                    : ChunkPacketType::Load;

            writer.Write(wireType);
            writer.Write(packet.coords);

            writer.Write(static_cast<uint32_t>(cChunk.palette.size()));
            writer.WriteRaw(
                cChunk.palette.data(),
                cChunk.palette.size() * sizeof(uint32_t));

            if (wireType == ChunkPacketType::LoadCompressed)
            {
                writer.Write(static_cast<uint32_t>(compressedData.size()));
                writer.WriteRaw(compressedData.data(), compressedData.size());
            }
            else
            {
                writer.WriteRaw(cChunk.data, CHUNK_SIZE_TOTAL);
            }

            break;
        }

        case ChunkPacketType::Update:
        {
            writer.Write(ChunkPacketType::Update);
            writer.Write(packet.coords);

            writer.Write(packet.dirtyCnt);

            for (uint8_t i = 0; i < packet.dirtyCnt; ++i)
            {
                writer.Write(packet.dirtyBlocks[i].position);
                writer.Write(packet.dirtyBlocks[i].block);
            }

            break;
        }

        case ChunkPacketType::Discard:
        {
            writer.Write(ChunkPacketType::Discard);
            writer.Write(packet.coords);
            break;
        }

        case ChunkPacketType::LoadCompressed:
        {
            /*
                The stream should not need to emit this directly.
                A logical Load is enough; serialization decides whether the
                wire representation becomes LoadCompressed.

                If this case ever appears anyway, handle it exactly like Load.
            */

            assert(packet.chunk != nullptr);

            PaletteCompressedChunk cChunk;
            PaletteCompressedChunk::FromChunkData(*packet.chunk, cChunk);

            auto compressedData =
                CompressChunk(
                    reinterpret_cast<const std::uint8_t*>(cChunk.data),
                    CHUNK_SIZE_TOTAL);

            writer.Write(ChunkPacketType::LoadCompressed);
            writer.Write(packet.coords);

            writer.Write(static_cast<uint32_t>(cChunk.palette.size()));
            writer.WriteRaw(
                cChunk.palette.data(),
                cChunk.palette.size() * sizeof(uint32_t));

            writer.Write(static_cast<uint32_t>(compressedData.size()));
            writer.WriteRaw(compressedData.data(), compressedData.size());

            break;
        }

        default:
        {
            assert(false);
            break;
        }
    }
}

static void Deserialize(Buffer& reader, TerrainChunkPacket& packet)
{
    using namespace game::terrain;
    using namespace game::net;

    packet = {};

    reader.Read(packet.type);
    reader.Read(packet.coords);

    switch (packet.type)
    {
        case ChunkPacketType::Load:
        case ChunkPacketType::LoadCompressed:
        {
            uint32_t paletteSize = 0;
            reader.Read(paletteSize);

            PaletteCompressedChunk cChunk;
            cChunk.palette.resize(paletteSize);

            reader.ReadRaw(
                cChunk.palette.data(),
                paletteSize * sizeof(uint32_t));

            if (packet.type == ChunkPacketType::Load)
            {
                reader.ReadRaw(cChunk.data, CHUNK_SIZE_TOTAL);
            }
            else
            {
                uint32_t compressedSize = 0;
                reader.Read(compressedSize);

                std::pmr::vector<uint8_t> compressedData(compressedSize);
                reader.ReadRaw(compressedData.data(), compressedSize);

                DecompressChunk(
                    compressedData.data(),
                    compressedSize,
                    cChunk.data,
                    CHUNK_SIZE_TOTAL);
            }

            cChunk.ToChunkData(packet.decodedStorage);
            packet.decodedStorage.Coords = packet.coords;
            packet.decodedStorage.state = ChunkState::Persistent;
            packet.chunk = &packet.decodedStorage;
            break;
        }

        case ChunkPacketType::Update:
        {
            reader.Read(packet.dirtyCnt);

            assert(packet.dirtyCnt <= packet.dirtyBlocks.size());

            if (packet.dirtyCnt > packet.dirtyBlocks.size())
            {
                packet.dirtyCnt =
                    static_cast<uint8_t>(packet.dirtyBlocks.size());
            }

            for (uint8_t i = 0; i < packet.dirtyCnt; ++i)
            {
                reader.Read(packet.dirtyBlocks[i].position);
                reader.Read(packet.dirtyBlocks[i].block);
            }

            packet.chunk = nullptr;
            break;
        }

        case ChunkPacketType::Discard:
        {
            packet.chunk = nullptr;
            packet.dirtyCnt = 0;
            break;
        }

        default:
        {
            assert(false);
            packet = {};
            break;
        }
    }
}
};

} // namespace oge::net
