#pragma once

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

using ReplicationTick = uint32_t;

using PeerId = uint32_t;
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
    LogCursor begin{};

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

struct ReplicationCapability : ICapability
{
    FamilyId family{};
    ReplicationMethod sendType = ReplicationMethod::SingleReliable;

    // we assume any type T with this capability are defined for
    //  net::Serialize(T) and net::Deserialize(T)

    // this install a hook that collects information from world
    //  and submit to the event log stream in world.ctx()
    //  on update (entity(ReplicatedTag)/component/chunk/block)
    using InstallHooksFn = void (*)(EventLogStream<>&,
                                    oge::runtime::OgeRegistryRef);
    InstallHooksFn installHooks = nullptr;

    // this apply a event in the event log stream
    using ApplyFn = void (*)(EventLogStream<>&, oge::runtime::OgeRegistryRef,
                             net::Buffer&);
    ApplyFn apply = nullptr;
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

    cap.apply = [](EventLogStream<>& /*stream*/,
                   oge::runtime::OgeRegistryRef world, net::Buffer& buffer)
    {
        TEvent event{};
        net::Deserialize(buffer, event);
        ApplyEvent(world, event);
    };

    return cap;
}

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
// A basic scheduler that packs as many events as possible into a single tick
// while respecting a configurable byte limit.
//
// Set m_maxBytesPerFrame to 0 for unlimited.
// ---------------------------------------------------------------------------

class SimplePacketScheduler : public PacketScheduler
{
   public:
    size_t m_maxBytesPerFrame = 1200;  // typical MTU-safe default

    SimplePacketScheduler() = default;

    explicit SimplePacketScheduler(size_t maxBytesPerFrame)
        : m_maxBytesPerFrame(maxBytesPerFrame)
    {
    }

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
        size_t usedBytes = 0;
        const size_t headerBytes =
            sizeof(FamilyId) + sizeof(LogCursor) + sizeof(uint32_t);

        while (true)
        {
            EventLogEntryConstRef ref{{}, m_scratchPayload};
            if (!stream.PeekEvent(ctx.peer.id, ref, cursor))
            {
                break;
            }

            // Estimate: per-packet header + payload size prefix + payload
            size_t entryBytes = headerBytes +
                                EventLogStream<>::PayloadSizePrefixBytes() +
                                ref.payload.size();

            if (m_maxBytesPerFrame > 0 &&
                usedBytes + entryBytes > m_maxBytesPerFrame)
            {
                break;
            }

            PacketDesc desc{};
            desc.logPosition = ref.entry.cursor;
            desc.payloadByteCount = ref.payload.size();
            plan.packets.push_back(desc);

            usedBytes += entryBytes;
            cursor = ref.entry.cursor + 1;
        }

        return !plan.packets.empty();
    }

   private:
    mutable std::vector<std::byte> m_scratchPayload;
};

using Tick = int64_t;

struct AdvanceTick
{
    Tick tick;
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

    void ProduceAll(oge::runtime::NetPacketSender& server,
                    oge::runtime::OgeRegistryRef world)
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
                    auto buffer = net::Buffer{entry.payload};
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
                        m_eventStream->SerializeEventPayload(entry.entry.cursor, packet,
                                                             entry.payload);
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
                        m_eventStream->SerializeEventPayload(entry.entry.cursor, packet,
                                                             entry.payload);
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
                            entry.entry.cursor, payloadPacket, entry.payload);
                        server.Send(peerState.peer.peer, payloadPacket,
                                    SendType::Reliable, 1);
                        break;
                    }
                }
            }
        }
    }

    void HandleIncoming(PeerId peer, oge::runtime::OgeRegistryRef world,
                        net::Buffer& buffer)
    {
        OGE_ASSERT(m_eventStream != nullptr, "EventStream is null");
        m_eventStream->DeserializeEvent(peer, buffer);
    }

    void AdvancePeerTick(PeerId peer, oge::runtime::OgeRegistryRef world)
    {
        ++m_currentTick;
    }

    // Store component-type info so the snapshot can iterate components.
    using SnapshotComponentFn = void (*)(EventLogStream<>&,
                                         oge::runtime::OgeRegistryRef,
                                         oge::runtime::OgeRegistryRef::Entity,
                                         const std::bitset<64>& peerMask);

    std::vector<SnapshotComponentFn> m_snapshotFns;

    template <typename T>
    static void SnapshotComponent(EventLogStream<>& stream,
                                  oge::runtime::OgeRegistryRef world,
                                  oge::runtime::OgeRegistryRef::Entity e,
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
    void GenerateSnapshot(PeerId peerId, oge::runtime::OgeRegistryRef world)
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
                evt.blocks.resize(terrain::CHUNK_SIZE_TOTAL);
                for (size_t i = 0; i < terrain::CHUNK_SIZE_TOTAL; ++i)
                {
                    evt.blocks[i] = chunk->data[i];
                }

                auto buf = m_eventStream->EnqueueEvent(
                    entt::type_hash<AddChunkEvent>::value(), net::Size(evt),
                    peerMask);
                net::Serialize(buf, evt);
            }
        }
    }

    void AddPeer(PeerId id, ENetPeer* peer,
                 oge::runtime::OgeRegistry* world = {})
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

DECL_NET_OBJ(game::net::AdvanceTick, { visit(self.tick); })
