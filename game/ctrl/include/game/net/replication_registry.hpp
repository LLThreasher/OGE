#pragma once

#include <array>
#include <bitset>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <unordered_map>
#include <vector>

#include "game/app_context.hpp"
#include "game/input/entity_event_stream.hpp"
#include "game/net/event_log_stream.hpp"
#include "game/net/replication_events.hpp"
#include "oge/assert.hpp"
#include "oge/event_stream.hpp"
#include "oge/event_stream2.hpp"
#include "oge/runtime/net_packet_sender.hpp"
#include "oge/runtime/net_serializer.hpp"
#include "oge/runtime/net_server.hpp"
#include "oge/runtime/net_traits.hpp"
#include "oge/runtime/oge_registry.hpp"
#include "oge/runtime/type_name.hpp"
#include "oge/runtime/typed_registry.hpp"

struct _ENetPeer;
typedef _ENetPeer ENetPeer;

namespace game::net
{

using oge::runtime::FamilyId;
using oge::runtime::ICapability;
using oge::runtime::oge_id_type;
using oge::runtime::SendType;
using oge::runtime::TypeRegistry;
namespace net = oge::runtime::net;
using input::EntityEvent;
using input::EntityEventStream;
using input::EntityEventType;
using oge::DiscreteEventStream;
using oge::NetworkEventStream;

using oge::runtime::OgeRegistryPtr;

using ReplicationTick = uint32_t;

// PeerId and Tick are defined in replication_events.hpp (included above).

struct NetPeer
{
    ENetPeer* peer = nullptr;
    PeerId id = 0;
};

// -----------------------------------------------------------------------------
// Packet planning
// -----------------------------------------------------------------------------
struct PacketDesc
{
    LogCursor logPosition;
    size_t payloadByteCount;
};

struct PacketPlan
{
    bool hasPacket = false;
    std::vector<PacketDesc> packets;
};

// -----------------------------------------------------------------------------
// Encode/decode/tick contexts
// -----------------------------------------------------------------------------
struct EncodeContext
{
    NetPeer peer{};

    // Cursor to start reading from in the event log.
    LogCursor begin = 0;

    // maximum number of bytes to send
    size_t byteLimit = std::numeric_limits<size_t>::max();
};

struct DecodeContext
{
    NetPeer peer{};

    // Cursor within this tick.
    LogCursor begin{};

    // Number of logical records/items contained in this packet.
    size_t itemCount = 0;
};

// -----------------------------------------------------------------------------
// Replication capability
// -----------------------------------------------------------------------------
enum class ReplicationMethod
{
    SingleReliable,
    SingleSequenced,
    StreamReliable,
};

constexpr size_t MAX_WORLD_VARIANTS = 8;

struct ReplicationCapability : ICapability
{
    FamilyId family{};
    ReplicationMethod sendType = ReplicationMethod::SingleReliable;

    // we assume any type T with this capability are defined for
    //  net::Serialize(T) and net::Deserialize(T)

    // this install a hook that collects information from world
    //  and submit to the event log stream in world.ctx()
    //  on update (entity(ReplicatedTag)/component/chunk/block)
    using InstallHooksFn = void (*)(EventLogStream<>&, OgeRegistryRef);
    InstallHooksFn installHooks = nullptr;

    // this apply a event in the event log stream
    using ApplyFn = void (*)(EventLogStream<>&, OgeRegistryRef, net::Buffer&);
    ApplyFn apply = nullptr;

    std::bitset<MAX_WORLD_VARIANTS> worldMask = {};
};

// =========================================================================
// ReplicationCapability factory helpers
//
// Each event type gets its own capability with its own family id.
// The family id matches the event type hash so that
// ReplicationRegistry::ProduceAll can look up the correct capability
// from the event log entry's type id.
// =========================================================================

template <typename TEvent>
ReplicationCapability MakeSimpleReplicationCapability(
    FamilyId family,
    typename ReplicationCapability::InstallHooksFn installHooks,
    ReplicationMethod sendType = ReplicationMethod::SingleReliable)
{
    ReplicationCapability cap{};
    cap.family = family;
    cap.sendType = sendType;
    cap.installHooks = installHooks;
    cap.worldMask.set(0);  // default world

    cap.apply = [](EventLogStream<>& /*stream*/, OgeRegistryRef world,
                   net::Buffer& buffer)
    {
        TEvent event{};
        net::Deserialize(buffer, event);
        ApplyEvent(world, event);
    };

    return cap;
}

class WorldRouter
{
    std::array<OgeRegistryPtr, MAX_WORLD_VARIANTS> m_worlds = {};

   public:
    WorldRouter(OgeRegistryRef ref)
    {
        m_worlds[0] = &ref;
    }

    void AddWorldVariant(size_t idx, OgeRegistryRef world)
    {
        OGE_ASSERT(m_worlds[idx] == nullptr, "world variant already exists");
        OGE_ASSERT(0 <= idx && idx < MAX_WORLD_VARIANTS, "invalid idx");
        m_worlds[idx] = &world;
    }

    template <typename Fn>
    void ApplyWorldFn(ReplicationCapability cap, Fn&& fn)
    {
        for (auto i = 0; i < MAX_WORLD_VARIANTS; ++i)
        {
            if (m_worlds[i] != nullptr && cap.worldMask.test(i))
            {
                fn(*m_worlds[i]);
            }
        }
    }

    // Store the incoming peer id in each world that has an IncomingPeerId
    // ctx slot, before the event's apply fn runs.  Apply fns are stateless
    // (function pointers), so per-apply context must come from the world.
    void SetIncomingPeerId(PeerId peerId)
    {
        for (size_t i = 0; i < MAX_WORLD_VARIANTS; ++i)
        {
            if (m_worlds[i] != nullptr &&
                (*m_worlds[i]).ctx().contains<IncomingPeerId>())
            {
                (*m_worlds[i]).ctx().get<IncomingPeerId>().id = peerId;
            }
        }
    }
};

class PacketScheduler
{
   public:
    virtual ~PacketScheduler() = default;
    virtual void Reset()
    {
    }
    virtual bool Schedule(const EventLogStream<>& stream,
                          const EncodeContext& ctx, PacketPlan& plan) = 0;
};

// ---------------------------------------------------------------------------
// SimplePacketScheduler
//
// A basic scheduler that packs as many events as possible into a single tick.
// There is no byte limit — ENet fragments reliable packets transparently, so
// oversized events (e.g. full chunk blocks) are safe to send in one packet.
// ---------------------------------------------------------------------------

class SimplePacketScheduler : public PacketScheduler
{
   public:
    SimplePacketScheduler() = default;

    void Reset() override
    {
        // no per-frame state to reset
    }

    bool Schedule(const EventLogStream<>& stream, const EncodeContext& ctx,
                  PacketPlan& plan) override
    {
        plan = {};
        plan.hasPacket = true;

        LogCursor cursor = ctx.begin;

        while (true)
        {
            EventLogEntryConstRef ref{{}, m_scratchPayload};
            if (!stream.PeekEvent(ctx.peer.id, ref, cursor))
            {
                break;
            }

            PacketDesc desc{};
            desc.logPosition = ref.entry.cursor;
            desc.payloadByteCount = ref.payload.size();
            plan.packets.push_back(desc);

            cursor = ref.entry.cursor + 1;
        }

        return !plan.packets.empty();
    }

   private:
    mutable SmallPayload m_scratchPayload;
};

struct AdvanceTick
{
    Tick tick;
    LogCursor peerCursor;
};

class ReplicationRegistry
{
    enum class SchedulerResult
    {
        Success,
        Failure,
        End,
    };

    struct PeerState
    {
        // Outbound cursor per family.
        //
        // This cursor is per-peer and per-family.
        //
        // If you need one cursor per tick, that is stream-specific and should
        // live inside the stream, not here.
        Tick tick = 0;
        NetPeer peer = {nullptr, 0};
    };

    std::unordered_map<FamilyId, ReplicationCapability*> m_units;
    std::unordered_map<PeerId, PeerState> m_peers;

    std::unique_ptr<PacketScheduler> m_scheduler = nullptr;

    EventLogStream<>* m_eventStream = nullptr;
    oge_id_type m_tickEventTypeId = 0;

    Tick m_currentTick = 0;

   public:
    struct Def
    {
        EventLogStream<>& stream;
        AnythingFactory& af;
    };

    ReplicationRegistry(const Def& def)
        : m_eventStream(&def.stream),
          m_tickEventTypeId(def.af.Id<AdvanceTick>()),
          m_scheduler(std::make_unique<SimplePacketScheduler>())
    {
    }

    Tick CurrentTick() const
    {
        return m_currentTick;
    }

    void RegisterFrom(TypeRegistry& types)
    {
        for (auto& type : types.GetAll())
        {
            if (auto* cap = type.capabilities.Get<ReplicationCapability>())
            {
                m_units[cap->family] = cap;
            }
        }
    }

    void ProduceAll(oge::runtime::NetPacketSender& server, OgeRegistryRef world)
    {
        OGE_ASSERT(m_eventStream != nullptr, "EventStream is null");

        for (auto& [peerId, peerState] : m_peers)
        {
            EncodeContext ectx{};
            ectx.peer = peerState.peer;

            PacketPlan plan;
            if (!m_scheduler->Schedule(*m_eventStream, ectx, plan))
            {
                continue;
            }

            for (auto& pdesc : plan.packets)
            {
                EventLogEntry entry;
                auto ok = m_eventStream->TryDequeueEvent(peerId, entry,
                                                         pdesc.logPosition);

                if (!ok) continue;

                if (entry.entry.id == m_tickEventTypeId)
                {
                    AdvanceTick tick;
                    auto buffer =
                        net::Buffer{entry.payload.data(), entry.payload.size()};
                    buffer.ToReadOnly();
                    net::Deserialize(buffer, tick);
                    peerState.tick = tick.tick;
                }

                auto it = m_units.find(entry.entry.id);
                if (it == m_units.end()) continue;

                auto* cap = it->second;
                switch (cap->sendType)
                {
                    case ReplicationMethod::SingleReliable:
                    {
                        auto packet = server.StartPacket(
                            m_eventStream->MetaSize() +
                            m_eventStream->PayloadHeaderBytes() +
                            pdesc.payloadByteCount);
                        m_eventStream->SerializeEventMeta(packet, entry.entry);
                        m_eventStream->SerializeEventPayload(
                            entry.entry.cursor, packet,
                            std::span<const std::byte>(entry.payload.data(),
                                                       entry.payload.size()));
                        server.Send(peerState.peer.peer, packet,
                                    SendType::Reliable, 0);
                        break;
                    }
                    case ReplicationMethod::SingleSequenced:
                    {
                        auto packet = server.StartPacket(
                            m_eventStream->MetaSize() +
                            m_eventStream->PayloadHeaderBytes() +
                            pdesc.payloadByteCount);
                        m_eventStream->SerializeEventMeta(packet, entry.entry);
                        m_eventStream->SerializeEventPayload(
                            entry.entry.cursor, packet,
                            std::span<const std::byte>(entry.payload.data(),
                                                       entry.payload.size()));
                        server.Send(peerState.peer.peer, packet,
                                    SendType::Sequenced, 0);
                        break;
                    }
                    case ReplicationMethod::StreamReliable:
                    {
                        auto packet =
                            server.StartPacket(m_eventStream->MetaSize());
                        m_eventStream->SerializeEventMeta(packet, entry.entry);
                        server.Send(peerState.peer.peer, packet,
                                    SendType::Reliable, 0);
                        auto payloadPacket = server.StartPacket(
                            m_eventStream->PayloadHeaderBytes() +
                            pdesc.payloadByteCount);
                        m_eventStream->SerializeEventPayload(
                            entry.entry.cursor, payloadPacket,
                            std::span<const std::byte>(entry.payload.data(),
                                                       entry.payload.size()));
                        server.Send(peerState.peer.peer, payloadPacket,
                                    SendType::Reliable, 1);
                        break;
                    }
                }
            }
        }
    }

    void HandleIncoming(PeerId peer, OgeRegistryRef world, net::Buffer& buffer)
    {
        HandleIncoming(peer, WorldRouter{world}, buffer);
    }

    // Incoming events route through a WorldRouter: the default world always
    // receives them, and worlds added as variants receive only the families
    // flagged in their ReplicationCapability::worldMask.  ClientScene2 uses
    // this to mirror server truth into a separate authoritative world that
    // owns the rollback snapshots.
    void HandleIncoming(PeerId peer, WorldRouter worldRouter,
                        net::Buffer& buffer)
    {
        OGE_ASSERT(m_eventStream != nullptr, "EventStream is null");

        // Covers both the direct event path and the split-packet payload
        // path below.
        worldRouter.SetIncomingPeerId(peer);

        auto meta = m_eventStream->DeserializeEvent(peer, buffer);

        // Split packets (StreamReliable): the payload-only half carries the
        // send-payload type id and no event id — apply once its meta half
        // has arrived.
        if (m_eventStream->IsSendPayloadType(meta.id))
        {
            const EventLogEntryMeta* entry =
                m_eventStream->GetEntry(meta.cursor);
            if (entry != nullptr) ApplyReceivedEvent(*entry, worldRouter);
            return;
        }

        ApplyReceivedEvent(meta, worldRouter);
    }

    // Deserialize the payload of a received event and apply it to the world
    // through the event family's ReplicationCapability::apply function.
    void ApplyReceivedEvent(const EventLogEntryMeta& meta,
                            WorldRouter worldRouter)
    {
        auto it = m_units.find(meta.id);
        if (it == m_units.end() || it->second->apply == nullptr) return;

        SmallPayload* payload = m_eventStream->GetPayload(meta.cursor);
        if (payload == nullptr) return;  // payload half still in flight

        worldRouter.ApplyWorldFn(
            *it->second, [&](OgeRegistryRef world)
            {
                // Fresh read-only buffer per world: Deserialize advances the
                // buffer's read cursor, so reusing one buffer across worlds
                // would consume the payload before the variant world saw it.
                net::Buffer buf{payload->data(), payload->size()};
                buf.ToReadOnly();
                it->second->apply(*m_eventStream, world, buf);
            });
    }

    // Enqueue one AdvanceTick event (full peer mask) so every connected
    // peer advances its tick.  On the client the tick apply drives the
    // rollback snapshot cadence.
    void AdvancePeerTick()
    {
        ++m_currentTick;
        // Capture the cursor where this AdvanceTick will land: the client
        // uses it (via the rollback pong) to map server log positions to
        // ticks.
        LogCursor cursor = m_eventStream->HeadCursor();
        AdvanceTick evt{m_currentTick, cursor};
        auto buf =
            m_eventStream->EnqueueEvent(m_tickEventTypeId, net::Size(evt));
        net::Serialize(buf, evt);
    }

    // Store component-type info so the snapshot can iterate components.
    using SnapshotComponentFn = void (*)(EventLogStream<>&, OgeRegistryRef,
                                         OgeRegistryRef::Entity,
                                         const std::bitset<64>& peerMask);

    std::vector<SnapshotComponentFn> m_snapshotFns;

    template <typename T>
    static void SnapshotComponent(EventLogStream<>& stream,
                                  OgeRegistryRef world,
                                  OgeRegistryRef::Entity e,
                                  const std::bitset<64>& peerMask)
    {
        if (!world.all_of<T>(e)) return;

        AddComponentEvent<T> evt{e, world.get<T>(e)};
        auto buf =
            stream.EnqueueEvent(entt::type_hash<AddComponentEvent<T>>::value(),
                                net::Size(evt), peerMask);
        net::Serialize(buf, evt);
    }

    // Register a component type for snapshot generation.
    template <typename T>
    void RegisterSnapshotComponent()
    {
        m_snapshotFns.push_back(&SnapshotComponent<T>);
    }

    // Generate a snapshot of the current world state for a newly joined
    // peer.  Pushes AddEntity / AddComponent / AddChunk events with a
    // receive mask that only targets this peer.
    void GenerateSnapshot(PeerId peerId, OgeRegistryRef world)
    {
        OGE_ASSERT(m_eventStream != nullptr, "EventStream is null");

        std::bitset<64> peerMask{};
        peerMask.set(peerId);

        // --- Entities ---
        auto view = world.view<ReplicatedTag>();
        for (entt::entity e : view)
        {
            AddEntityEvent evt{e};
            auto buf = m_eventStream->EnqueueEvent(
                entt::type_hash<AddEntityEvent>::value(), net::Size(evt),
                peerMask);
            net::Serialize(buf, evt);

            // All registered component types.
            for (auto& fn : m_snapshotFns)
            {
                fn(*m_eventStream, world, e, peerMask);
            }
        }

        // --- Terrain chunks ---
        if (world.ctx().contains<terrain::TerrainView>())
        {
            auto& terrain = world.ctx().get<terrain::TerrainView>();

            terrain::ChunkHandle cursor{};
            while (const terrain::ChunkData* chunk = terrain.PollChunk(cursor))
            {
                if (chunk->state != terrain::ChunkState::Persistent)
                {
                    continue;
                }

                AddChunkEvent evt{};
                evt.coords = chunk->Coords;
                terrain::PaletteCompressedChunk::FromChunkData(*chunk,
                                                               evt.chunk);

                auto buf = m_eventStream->EnqueueEvent(
                    entt::type_hash<AddChunkEvent>::value(), net::Size(evt),
                    peerMask);
                net::Serialize(buf, evt);
            }
        }
    }

    void AddPeer(PeerId id, ENetPeer* peer, OgeRegistryPtr world = nullptr)
    {
        PeerState peerState{};
        peerState.peer = {peer, id};
        m_peers.emplace(id, std::move(peerState));

        m_eventStream->AddPeer(id);

        if (world != nullptr)
        {
            GenerateSnapshot(id, *world);
        }
    }

    void RemovePeer(PeerId peer)
    {
        m_peers.erase(peer);
        m_eventStream->RemovePeer(peer);
    }

    auto& Peers() const
    {
        return m_peers;
    }
};

void RegisterReplications(oge::runtime::AnythingFactory& af,
                          ReplicationRegistry& rf);

}  // namespace game::net

DECL_TYPE_NAME(game::net::AdvanceTick, "net::AdvanceTick")

DECL_NET_OBJ(game::net::AdvanceTick, {
    visit(self.tick);
    visit(self.peerCursor);
})
