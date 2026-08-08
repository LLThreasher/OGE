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

#include "entt/core/fwd.hpp"
#include "entt/entity/fwd.hpp"
#include "game/components.hpp"
#include "game/components_net.hpp"
#include "game/input/entity_event_stream.hpp"
#include "game/input/net.hpp"
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
using NetCursor = uint32_t;

using PeerId = uint32_t;
struct NetPeer
{
    ENetPeer* peer = nullptr;
    PeerId id = 0;
};

// -----------------------------------------------------------------------------
// Packet planning
// -----------------------------------------------------------------------------
struct PacketPlan
{
    bool hasPacket = false;

    /*
        Actual stream tick to write into the packet header.
    */
    ReplicationTick tick = 0;

    NetCursor begin{};
    NetCursor end{};

    /*
        Payload byte count only.
    */
    size_t byteCount = 0;
};

// -----------------------------------------------------------------------------
// Encode/decode/tick contexts
// -----------------------------------------------------------------------------
struct EncodeContext
{
    entt::any& gloablState;

    NetPeer peer{};

    /*
        Current tick of the sender.

        This is not necessarily the tick written into the packet header.
        The stream may use this to decide what to stage or flush.

        The actual packet tick is returned in PacketPlan::packetTick.
    */
    ReplicationTick senderTick = 0;
    ReplicationTick packetTick = 0;

    /*
        Cursor requested by the registry.

        This is per-peer and per-family at the registry level. Streams that
        maintain one cursor per tick may ignore this and return their own
        PacketPlan::begin / PacketPlan::end.
    */
    NetCursor begin{};

    // maximum number of bytes to send
    size_t byteLimit = std::numeric_limits<size_t>::max();

    entt::any peerState = nullptr;
};

struct DecodeContext
{
    NetPeer peer{};

    // Tick carried by the packet header.
    //
    // Insert should place the decoded payload into this tick's receive buffer.
    ReplicationTick tick = 0;

    // Cursor within this tick.
    NetCursor begin{};

    // Number of logical records/items contained in this packet.
    size_t itemCount = 0;
};

struct AdvanceTickContext
{
    entt::any& gloablState;

    // The tick the application/replication layer has advanced to.
    //
    // The stream can use this to:
    //   - publish completed received ticks
    //   - discard old OOO buffers
    //   - rotate snapshot history
    //   - generate outbound tick state
    ReplicationTick tick = 0;

    NetPeer peer{};
};

// -----------------------------------------------------------------------------
// Stream interface concepts
// -----------------------------------------------------------------------------
//
// Global stream:
//
//     PacketPlan Peek(const EncodeContext&) const;
//     bool Poll(const EncodeContext&, T::Event&) const;
//     bool Insert(const DecodeContext&, const T::Event&);
//     void AdvanceTick(const AdvanceTickContext&);
//
// Entity stream:
//
//     PacketPlan Peek(entt::entity, const EncodeContext&) const;
//     bool Poll(entt::entity, const EncodeContext&, T::Event&) const;
//     bool Insert(entt::entity, const DecodeContext&, const T::Event&);
//     void AdvanceTick(entt::entity, const AdvanceTickContext&);
//
// `Insert` is intentionally named insert instead of push because inbound
// packets may arrive out of order.
//
// The stream decides what to do with:
//
//   - duplicate packet
//   - stale packet
//   - future packet
//   - partially received tick
//   - complete tick
//   - packet outside retention window
//
// -----------------------------------------------------------------------------
template <typename T>
concept IsNetOutputStream =
    requires(const T& cs, const EncodeContext& ectx,
             const AdvanceTickContext& tctx, typename T::Packet& packet) {
        typename T::Packet;

        { cs.Peek(ectx) } -> std::same_as<PacketPlan>;
        { cs.Poll(ectx, packet) } -> std::same_as<bool>;
        { cs.AdvanceTick(tctx) } -> std::same_as<void>;
    };

template <typename T>
concept IsNetInputStream = requires(T& s, const DecodeContext& dctx,
                                    const typename T::Packet& packet) {
    typename T::Packet;

    { s.Insert(dctx, packet) } -> std::same_as<bool>;
};

// -----------------------------------------------------------------------------
// Registry-backed stream adapters
// -----------------------------------------------------------------------------
template <typename TStream>
class PeerStreamStore
{
   private:
    std::unordered_map<PeerId, TStream> m_streams;

   public:
    TStream* TryGet(PeerId peer)
    {
        auto it = m_streams.find(peer);

        if (it == m_streams.end())
        {
            return nullptr;
        }

        return &it->second;
    }

    const TStream* TryGet(PeerId peer) const
    {
        auto it = m_streams.find(peer);

        if (it == m_streams.end())
        {
            return nullptr;
        }

        return &it->second;
    }

    template <typename... TArgs>
    TStream& Emplace(PeerId peer, TArgs&&... args)
    {
        auto [it, inserted] =
            m_streams.try_emplace(peer, std::forward<TArgs>(args)...);

        return it->second;
    }

    void Erase(PeerId peer)
    {
        m_streams.erase(peer);
    }

    bool Contains(PeerId peer) const
    {
        return m_streams.contains(peer);
    }
};

template <typename TStream>
    requires IsNetOutputStream<TStream>
struct RegistryOutputStream
{
    using Stream = TStream;
    using Packet = typename TStream::Packet;
    using Store = PeerStreamStore<TStream>;

    static PacketPlan Peek(const entt::registry& world,
                           const EncodeContext& ctx)
    {
        const auto& store = world.ctx().template get<Store>();

        const TStream* stream = store.TryGet(ctx.peer.id);

        if (stream == nullptr)
        {
            return PacketPlan{};
        }

        return stream->Peek(ctx);
    }

    static bool Poll(const entt::registry& world, const EncodeContext& ctx,
                     net::Buffer& buf)
    {
        const auto& store = world.ctx().template get<Store>();

        const TStream* stream = store.TryGet(ctx.peer.id);

        if (stream == nullptr)
        {
            return false;
        }

        Packet packet{};

        if (!stream->Poll(ctx, packet))
        {
            return false;
        }

        net::Serialize(buf, packet);
        return true;
    }

    static void AdvanceTick(entt::registry& world,
                            const AdvanceTickContext& ctx)
    {
        auto& store = world.ctx().template get<Store>();

        TStream* stream = store.TryGet(ctx.peer.id);

        if (stream == nullptr)
        {
            return;
        }

        stream->AdvanceTick(ctx);
    }
};

template <typename TStream>
    requires IsNetInputStream<TStream>
struct RegistryInputStream
{
    using Stream = TStream;
    using Packet = typename TStream::Packet;
    using Store = PeerStreamStore<TStream>;

    static bool Insert(entt::registry& world, const DecodeContext& ctx,
                       net::Buffer& buf)
    {
        auto& store = world.ctx().template get<Store>();

        TStream* stream = store.TryGet(ctx.peer.id);

        if (stream == nullptr)
        {
            return false;
        }

        Packet packet{};
        net::Deserialize(buf, packet);

        return stream->Insert(ctx, packet);
    }
};

// -----------------------------------------------------------------------------
// Replication capability
// -----------------------------------------------------------------------------
struct ReplicationCapability : ICapability
{
    using PrepareFn = PacketPlan (*)(const ReplicationCapability&,
                                     const entt::registry&,
                                     const EncodeContext&);

    using EncodeFn = bool (*)(const ReplicationCapability&,
                              const entt::registry&, const EncodeContext&,
                              net::Buffer&);

    using DecodeFn = bool (*)(const ReplicationCapability&, entt::registry&,
                              const DecodeContext&, net::Buffer&);

    using AdvanceTickFn = void (*)(const ReplicationCapability&,
                                   entt::registry&, const AdvanceTickContext&);
    
    using InitGlobalFn = entt::any (*)(entt::registry&);

    using InitPeerFn = entt::any (*)(entt::any&);

    FamilyId family{};

    SendType sendType = SendType::Unreliable;
    uint8_t channel = 0;

    PrepareFn prepare = nullptr;
    EncodeFn encode = nullptr;
    DecodeFn decode = nullptr;

    AdvanceTickFn advanceOutputTick = nullptr;
};

template <typename TOutputStream, typename TInputStream>
    requires IsNetOutputStream<TOutputStream> &&
             IsNetInputStream<TInputStream> &&
             std::same_as<typename TOutputStream::Packet,
                          typename TInputStream::Packet>
static ReplicationCapability MakeReplicationCapability(
    FamilyId family, SendType sendType = SendType::Unreliable,
    uint8_t channel = 0)
{
    ReplicationCapability cap{};
    cap.family = family;
    cap.sendType = sendType;
    cap.channel = channel;

    cap.prepare = [](const ReplicationCapability&, const entt::registry& world,
                     const EncodeContext& ctx) -> PacketPlan
    { return RegistryOutputStream<TOutputStream>::Peek(world, ctx); };

    cap.encode = [](const ReplicationCapability&, const entt::registry& world,
                    const EncodeContext& ctx, net::Buffer& buf) -> bool
    { return RegistryOutputStream<TOutputStream>::Poll(world, ctx, buf); };

    cap.decode = [](const ReplicationCapability&, entt::registry& world,
                    const DecodeContext& ctx, net::Buffer& buf) -> bool
    { return RegistryInputStream<TInputStream>::Insert(world, ctx, buf); };

    cap.advanceOutputTick = [](const ReplicationCapability&,
                               entt::registry& world,
                               const AdvanceTickContext& ctx)
    { RegistryOutputStream<TOutputStream>::AdvanceTick(world, ctx); };

    return cap;
}

// -----------------------------------------------------------------------------
// Replication registry
// -----------------------------------------------------------------------------
//
// Public interface mostly preserved.
// Added:
//
//     AdvanceTick(world, tick)
//     AdvancePeerTick(peer, world, tick)
//
// Existing functions are unchanged in name.
// ProduceAll now takes a tick argument because the packet header includes tick.
//
// If you absolutely cannot change ProduceAll's signature, you can instead store
// the current outbound tick inside ReplicationRegistry and add SetTick(...).
//
// -----------------------------------------------------------------------------

class PacketScheduler
{
   public:
    virtual PacketPlan ScheduleTick(const ReplicationCapability* cap,
                                    const EncodeContext& ctx) = 0;
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
        std::unordered_map<FamilyId, NetCursor> cursor;
        ENetPeer* peer;
    };

    std::vector<oge_id_type> m_serializableComponents;
    std::vector<std::tuple<FamilyId, entt::any>> m_sendUnits;
    std::unordered_map<FamilyId, ReplicationCapability*> m_units;
    std::unordered_map<PeerId, PeerState> m_peers;

    std::unique_ptr<PacketScheduler> m_scheduler;

   public:
    const std::vector<oge_id_type>& SeralizableComponents() const
    {
        return m_serializableComponents;
    }

    template <typename T>
    void RegisterSerializableComponents()
    {
        m_serializableComponents.push_back(entt::type_hash<T>::value());
    }

    void AddFamilyToSend(FamilyId id)
    {
        assert(m_peers.empty());
        m_sendUnits.push_back(id);
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
                    entt::registry& world, ReplicationTick senderTick)
    {
        constexpr size_t HeaderBytes = sizeof(FamilyId) +
                                       sizeof(ReplicationTick) +
                                       sizeof(NetCursor) + sizeof(uint8_t);

        constexpr size_t MaxPacketBytes = 1200;
        static_assert(MaxPacketBytes > HeaderBytes);

        constexpr size_t MaxPayloadBytes = MaxPacketBytes - HeaderBytes;

        auto produceOneTick = [&](NetCursor& cursor, ReplicationCapability* cap,
                                  NetPeer peer,
                                  FamilyId family) -> SchedulerResult
        {
            EncodeContext ectx{};
            ectx.peer = peer;
            ectx.senderTick = senderTick;
            ectx.packetTick = 0;
            ectx.begin = cursor;

            PacketPlan plan = m_scheduler->ScheduleTick(cap, ectx);

            if (!plan.hasPacket) return SchedulerResult::End;

            auto buf = server.StartPacket(HeaderBytes + plan.byteCount);

            buf.Write<FamilyId>(family);
            buf.Write<ReplicationTick>(plan.tick);
            buf.Write<NetCursor>(plan.begin);
            buf.Write<uint8_t>(static_cast<uint8_t>(plan.end - plan.begin));

            for (size_t i = plan.begin; i < plan.end; ++i)
            {
                EncodeContext pollCtx = ectx;
                pollCtx.packetTick = plan.tick;
                pollCtx.begin = i;

                bool ok = cap->encode(*cap, world, pollCtx, buf);
                assert(ok);

                if (!ok)
                {
                    return SchedulerResult::Failure;
                }
            }

            server.Send(peer.peer, buf, cap->sendType, cap->channel);

            cursor = plan.end;
            return SchedulerResult::Success;
        };

        for (auto& [id, peerState] : m_peers)
        {
            for (auto family : m_sendUnits)
            {
                auto unitIt = m_units.find(family);
                if (unitIt == m_units.end())
                {
                    continue;
                }

                ReplicationCapability* cap = unitIt->second;
                if (!cap || !cap->prepare || !cap->encode)
                {
                    continue;
                }

                auto& cursor = peerState.cursor[family];

                do
                {
                    auto res = produceOneTick(cursor, cap, {peerState.peer, id},
                                              family);
                    if (res == SchedulerResult::End ||
                        res == SchedulerResult::Failure)
                        break;
                } while (true);
            }
        }
    }

    void HandleIncoming(PeerId peer, entt::registry& world, net::Buffer& buffer)
    {
        FamilyId family = buffer.Read<FamilyId>();
        ReplicationTick tick = buffer.Read<ReplicationTick>();
        NetCursor begin = buffer.Read<NetCursor>();
        uint8_t itemCount = buffer.Read<uint8_t>();

        auto it = m_units.find(family);
        if (it == m_units.end())
        {
            return;
        }

        ReplicationCapability* cap = it->second;
        if (!cap || !cap->decode)
        {
            return;
        }

        auto peerIt = m_peers.find(peer);
        if (peerIt == m_peers.end())
        {
            return;
        }

        DecodeContext dctx{};
        dctx.peer = {peerIt->second.peer, peer};
        dctx.tick = tick;
        dctx.begin = begin;
        dctx.itemCount = itemCount;

        cap->decode(*cap, world, dctx, buffer);
    }

    void AdvancePeerTick(PeerId peer, entt::registry& world,
                         ReplicationTick tick)
    {
        AdvanceTickContext ctx{};
        ctx.tick = tick;

        auto peerIt = m_peers.find(peer);
        if (peerIt == m_peers.end())
        {
            return;
        }

        PeerState& peerState = peerIt->second;
        ctx.peer = {peerState.peer, peer};

        for (auto family : m_sendUnits)
        {
            auto it = m_units.find(family);
            if (it == m_units.end())
            {
                continue;
            }

            ReplicationCapability* cap = it->second;
            if (!cap || !cap->advanceOutputTick)
            {
                continue;
            }

            cap->advanceOutputTick(*cap, world, ctx);
        }
    }

    void AddPeer(PeerId id, ENetPeer* peer)
    {
        PeerState peerState{};
        peerState.peer = peer;

        for (auto& [family, cap] : m_units)
        {
            peerState.cursor.emplace(family, NetCursor{});
        }

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
