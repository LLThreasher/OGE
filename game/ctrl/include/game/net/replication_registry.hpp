#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <limits>
#include <memory>
#include <optional>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include "entt/entity/fwd.hpp"
#include "game/app_context.hpp"
#include "game/components.hpp"
#include "game/components_net.hpp"
#include "game/input/entity_event_stream.hpp"
#include "game/input/net.hpp"
#include "game/net/event_log_stream.hpp"
#include "game/terrain/terrain_view.hpp"
#include "oge/event_stream.hpp"
#include "oge/event_stream2.hpp"
#include "oge/log.hpp"
#include "oge/runtime/net_packet_sender.hpp"
#include "oge/runtime/net_serializer.hpp"
#include "oge/runtime/net_server.hpp"
#include "oge/runtime/net_traits.hpp"
#include "oge/runtime/typed_registry.hpp"

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
    using InstallHooksFn = void (*) (EventLogStream<>&, entt::registry&);
    InstallHooksFn installHooks = nullptr;

    // this apply a event in the event log stream
    using ApplyFn = void (*) (EventLogStream<>&, entt::registry&, net::Buffer&);
    ApplyFn apply = nullptr;
};

class PacketScheduler
{
   public:
    virtual void Reset();
    virtual bool Schedule(const EventLogStream<>& stream,
                          const EncodeContext& ctx, PacketPlan& plan) = 0;
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

    EventLogStream<>& m_eventStream;
    oge_id_type m_tickEventTypeId;

    Tick m_currentTick = 0;

   public:
    struct Def
    {
        EventLogStream<>& stream;
        AnythingFactory& af;
    };

    ReplicationRegistry(const Def& def)
        : m_eventStream(def.stream), m_tickEventTypeId(def.af.Id<AdvanceTick>())
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
                    entt::registry& world)
    {
        for (auto& [id, peerState] : m_peers)
        {
            EncodeContext ectx{};
            ectx.peer = peerState.peer;

            PacketPlan plan;
            m_scheduler->Schedule(m_eventStream, ectx, plan);

            if (!plan.hasPacket) continue;

            for (auto pdesc : plan.packets)
            {
                EventLogEntry entry;
                auto ok = m_eventStream.TryDequeueEvent(id, entry);

                assert(ok);

                if (entry.entry.id == m_tickEventTypeId)
                {
                    AdvanceTick tick;
                    auto buffer = net::Buffer{entry.payload};
                    net::Deserialize(buffer, tick);
                    peerState.tick = tick.tick;
                }

                auto cap = m_units.at(entry.entry.id);
                switch (cap->sendType)
                {
                    case ReplicationMethod::SingleReliable:
                    {
                        auto packet = server.StartPacket(
                            m_eventStream.MetaSize() + pdesc.payloadByteCount);
                        m_eventStream.SerializeEventMeta(packet, entry.entry);
                        m_eventStream.SerializeEventPayload(packet,
                                                            entry.payload);
                        server.Send(peerState.peer.peer, packet, SendType::Reliable,
                                    0);
                        break;
                    }
                    case ReplicationMethod::SingleSequenced:
                    {
                        auto packet = server.StartPacket(
                            m_eventStream.MetaSize() + pdesc.payloadByteCount);
                        m_eventStream.SerializeEventMeta(packet, entry.entry);
                        m_eventStream.SerializeEventPayload(packet,
                                                            entry.payload);
                        server.Send(peerState.peer.peer, packet, SendType::Sequenced,
                                    0);
                        break;
                    }
                    case ReplicationMethod::StreamReliable:
                    {
                        {
                            auto packet =
                                server.StartPacket(m_eventStream.MetaSize());
                            m_eventStream.SerializeEventMeta(packet,
                                                             entry.entry);
                            server.Send(peerState.peer.peer, packet,
                                        SendType::Reliable, 0);
                        }
                        {
                            auto packet =
                                server.StartPacket(sizeof(entry.entry.cursor) +
                                                   pdesc.payloadByteCount);
                            net::Serialize(packet, entry.entry.cursor);
                            m_eventStream.SerializeEventPayload(packet,
                                                                entry.payload);
                            server.Send(peerState.peer.peer, packet,
                                        SendType::Reliable, 1);
                        }
                        break;
                    }
                }
            }
        }
    }

    void HandleIncoming(PeerId peer, entt::registry& world, net::Buffer& buffer)
    {
        m_eventStream.DeserializeEvent(peer, buffer);
    }

    void AdvancePeerTick(PeerId peer, entt::registry& world)
    {
        ++m_currentTick;
    }

    void AddPeer(PeerId id, ENetPeer* peer)
    {
        PeerState peerState{};
        peerState.peer = {peer, id};
        m_peers.emplace(id, std::move(peerState));
    }

    void RemovePeer(PeerId peer)
    {
        m_peers.erase(peer);
    }

    auto& Peers() const
    {
        return m_peers;
    }
};

void RegisterReplications(oge::runtime::AnythingFactory& af,
                          ReplicationRegistry& rf);

template <typename TComponent, size_t BufSize = 128>
void InstallComponentReplicationHooks(entt::registry& world)
{
}

}  // namespace game::net
