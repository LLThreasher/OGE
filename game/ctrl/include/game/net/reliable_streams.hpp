#pragma once

#include <concepts>
#include <cstddef>
#include <unordered_map>
#include <vector>

#include "entt/core/any.hpp"
#include "entt/core/fwd.hpp"
#include "entt/entity/fwd.hpp"
#include "game/events.hpp"
#include "game/input/entity_event_stream.hpp"
#include "game/net/replication_registry.hpp"
#include "game/terrain/terrain_view.hpp"
#include "oge/ring_buffer.hpp"
#include "oge/runtime/net_serializer.hpp"
#include "oge/runtime/net_traits.hpp"

namespace game::net
{
template <typename T>
concept IsReliableEventStream =
    requires(T& stream, const T& constStream, typename T::Cursor& cursor,
             typename T::Event& event, const size_t offset) {
        typename T::Event;
        typename T::Cursor;

        { constStream.PollOne(cursor, event) } -> std::same_as<bool>;
        { constStream.Head() } -> std::same_as<const typename T::Event>;
        { constStream.HeadCursor() } -> std::same_as<typename T::Cursor>;
        { stream.Push(event) };
    };

template <typename T>
concept IsEventStreamPacketAdapter =
    requires(T::Stream& stream, const T::Stream& cstream, T::Cursor& cursor,
             T::Packet& packet, const T::Packet& cpacket) {
        typename T::Stream;
        typename T::Cursor;
        typename T::Packet;

        { T::ExtractPacket(cstream, cursor, packet) } -> std::same_as<bool>;
        { T::InsertPacket(stream, cpacket) };
    } &&
    IsReliableEventStream<typename T::Stream>;

template <typename T>
concept IsSnapshotAdapter =
    requires(entt::registry& world,
             std::vector<typename T::Packet> packets) {
        typename T::Packet;

        { T::ExtractSnapshot(world, packets) };
    };

// in this class, we assume that we can extract packet without book-keeping.
// Tick starts from 0, 0 is always a noop tick, used for initalizing world
// state.
//  when a client joins at tick n, events on tick n is skipped, instead,
//  a snapshot will be created at tick n for the client. Then normal update
//  start from n+1
// Particularly in this class, only rpc style events are handled. Their effects
//  are temporary and won't affect persistent world state directly, or can be
//  safely ignored.
template <typename TAdapter>
    requires IsEventStreamPacketAdapter<TAdapter>
class BasicEventNetOutputStream
{
   public:
    using Stream = typename TAdapter::Stream;
    using Packet = typename TAdapter::Packet;
    using Cursor = typename TAdapter::Cursor;

   private:
    struct RingEntry
    {
        Cursor cursor{};
        NetCursor netCursor{};
    };

    using RingBuffer = oge::RingBuffer<RingEntry, 256>;

    struct PeerData
    {
        Cursor cursor{};
        ReplicationTick tick = RingBuffer::startIdx;
    };

    struct GlobalData
    {
        RingBuffer m_tickBuffer;
        Stream& m_stream;
    };

   public:
    struct Def
    {
        entt::registry& world;
    };

    static entt::any InitGlobalState(entt::registry& world)
    {
        auto res = GlobalData{
            {},
            world.ctx().get<Stream>(),
        };
        res.m_tickBuffer.Push({});
        return res;
    }

    static entt::any InitPeerState(entt::any& _global)
    {
        GlobalData& global = entt::any_cast<GlobalData&>(_global);
        return PeerData{
            global.m_stream.HeadCursor(),
            global.m_tickBuffer.HeadCursor(),
        };
    }

    static PacketPlan Peek(const EncodeContext& ectx)
    {
        const GlobalData& global = entt::any_cast<const GlobalData&>(ectx.gloablState);
        const PeerData& state = entt::any_cast<const PeerData&>(ectx.peerState);

        const NetCursor packetNumber = ectx.begin;

        ReplicationTick tick = state.tick;
        const auto head = global.m_tickBuffer.HeadCursor();

        while (tick < head)
        {
            const NetCursor tickEnd = global.m_tickBuffer.Get(tick).netCursor;

            if (packetNumber < tickEnd)
            {
                Cursor cursor = state.cursor;
                Packet event{};

                if (!TAdapter::ExtractPacket(global.m_stream, cursor, event))
                {
                    return {};
                }

                PacketPlan plan{};
                plan.hasPacket = true;
                plan.tick = tick;
                plan.byteCount = net::Size(event);
                plan.begin = packetNumber;
                plan.end = packetNumber + 1;

                return plan;
            }

            ++tick;
        }

        return {};
    }

    static bool Poll(EncodeContext& ectx, net::Buffer& buffer)
    {
        GlobalData& global = entt::any_cast<GlobalData&>(ectx.gloablState);
        PeerData& state = entt::any_cast<PeerData&>(ectx.peerState);

        Cursor cursor = state.cursor;

        Packet packet;
        if (TAdapter::ExtractPacket(global.m_stream, cursor, packet))
        {
            net::Serialize(buffer, packet);
            state.cursor = cursor;
            state.tick = ectx.packetTick;
            return true;
        }

        return false;
    }

    static void AdvanceTick(const AdvanceTickContext& tctx)
    {
        GlobalData& global = entt::any_cast<GlobalData&>(tctx.gloablState);

        // we check how many packets generated in the current tick
        RingEntry entry = global.m_tickBuffer.Head();
        Packet p;

        while (TAdapter::ExtractPacket(global.m_stream, entry.cursor, p))
        {
            entry.netCursor++;
        }

        global.m_tickBuffer.Push(entry);
    }
};

enum class LifetimeEventType
{
    Create,
    Update,
    Destroy,
};

template <typename TPayload>
struct LifetimeEvent
{
    LifetimeEventType type;
    TPayload payload;
};

template <typename T>
concept IsLifetimeEvent = requires(T event) {
    { event.type } -> std::same_as<LifetimeEventType>;
};

template <typename TAdapter>
    requires IsEventStreamPacketAdapter<TAdapter> &&
             IsLifetimeEvent<typename TAdapter::Packet>
class SnapshotReliableEventNetOutputStream
{
   public:
    using Base = BasicEventNetOutputStream<TAdapter>;

    using Stream = typename Base::Stream;
    using Event = typename Base::Event;
    using Cursor = typename Base::Cursor;
    using Packet = typename Base::Packet;

   private:
    struct SnapshotState
    {
        bool initialized = false;
        bool active = false;

        ReplicationTick tick = 0;

        std::vector<entt::entity> entities{};
        size_t entityIndex = 0;

        NetCursor netCursor{0};
    };

    struct PlannedSnapshotPacket
    {
        ReplicationTick tick = 0;
        NetCursor begin{};
        NetCursor end{};
        Packet packet{};
    };


    struct GlobalData
    {
        entt::registry& m_registry;
        Base m_baseStream;
        entt::any m_baseStates;
    };

    struct PeerData
    {
        SnapshotState snapshotState;
        entt::any basePeerData;
    };

   public:
    static entt::any InitGlobalState(entt::registry& world)
    {
        return GlobalData{
            world,
            {world},
            Base::InitGlobalState(world),
        };
    }

    static entt::any InitPeerState(entt::any& _global)
    {
        GlobalData& global = entt::any_cast<GlobalData&>(_global);
        return SnapshotState{
            {},
            Base::InitPeerState(global.m_baseStates),
        };
    }

    static PacketPlan Peek(const EncodeContext& ectx)
    {
        EnsureSnapshotInitialized(ectx.senderTick);

        if (m_snapshot.active)
        {
            SnapshotState copy = m_snapshot;

            PlannedSnapshotPacket planned{};
            if (!PeekNextSnapshotFromState(copy, planned))
            {
                /*
                    Do not mutate m_snapshot from Peek.

                    Let Poll deactivate the real snapshot state.
                */
                return Base::Peek(ectx);
            }

            const size_t byteCount = net::Size(planned.packet);

            PacketPlan plan{};
            plan.hasPacket = true;
            plan.tick = planned.tick;
            plan.begin = planned.begin;
            plan.end = planned.end;
            plan.byteCount = byteCount;

            return plan;
        }

        return Base::Peek(ectx);
    }

    bool Poll(const EncodeContext& ectx, Packet& packet) const
    {
        EnsureSnapshotInitialized(ectx.senderTick);

        if (m_snapshot.active)
        {
            PlannedSnapshotPacket planned{};
            if (PollNextSnapshotFromState(m_snapshot, planned))
            {
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

                const size_t byteCount = net::Size(planned.packet);
                if (ectx.maxPacketBytes != 0 && byteCount > ectx.maxPacketBytes)
                {
                    return false;
                }

                packet = planned.packet;
                return true;
            }

            /*
                Snapshot is finished.

                Because the base stream's default cursor starts at latest on
                first PollOne, the base stream starts from this point forward.
            */
            m_snapshot.active = false;
        }

        return Base::Poll(ectx, packet);
    }

    void AdvanceTick(const AdvanceTickContext& tctx) const
    {
        /*
            Do not advance the base stream during snapshot phase.

            Once snapshot is complete, let the original reliable stream behave
            exactly as before.
        */
        if (!m_snapshot.initialized || m_snapshot.active)
        {
            return;
        }

        Base::AdvanceTick(tctx);
    }

   private:
    static void EnsureSnapshotInitialized(GlobalData& self, ReplicationTick senderTick) const
    {
        if (self.m_snapshot.initialized)
        {
            return;
        }

        self.m_snapshot.initialized = true;
        self.m_snapshot.active = true;
        self.m_snapshot.tick = senderTick;
        self.m_snapshot.entityIndex = 0;
        self.m_snapshot.netCursor = NetCursor{0};
        self.m_snapshot.entities.clear();

        CaptureSnapshotEntities(self.m_snapshot.entities);

        /*
            If there are no snapshot entities, Poll() will deactivate snapshot
            mode and immediately delegate to the base stream.
        */
    }

    static void CaptureSnapshotEntities(GlobalData& self, std::vector<entt::entity>& entities) const
    {
        entities.clear();

        auto view = self.m_registry.template view<Component>();

        entities.reserve(view.size_hint());

        for (entt::entity entity : view)
        {
            entities.push_back(entity);
        }
    }

    bool PeekNextSnapshotFromState(SnapshotState& state,
                                   PlannedSnapshotPacket& planned) const
    {
        return PollNextSnapshotFromState(state, planned);
    }

    bool PollNextSnapshotFromState(SnapshotState& state,
                                   PlannedSnapshotPacket& planned) const
    {
        planned = {};

        while (state.entityIndex < state.entities.size())
        {
            const entt::entity entity = state.entities[state.entityIndex++];

            const Component* component =
                m_registry.template try_get<Component>(entity);

            if (component == nullptr)
            {
                continue;
            }

            Packet packet{};
            if (!BuildSnapshotPacket(entity, *component, packet))
            {
                continue;
            }

            const NetCursor begin = state.netCursor;
            const NetCursor end = AdvanceNetCursor(begin);

            state.netCursor = end;

            planned.tick = state.tick;
            planned.begin = begin;
            planned.end = end;
            planned.packet = packet;

            return true;
        }

        return false;
    }

    static NetCursor AdvanceNetCursor(NetCursor cursor)
    {
        return NetCursor{static_cast<std::uint32_t>(++cursor)};
    }

    static bool BuildSnapshotPacket(entt::entity entity,
                                    const Component& component, Packet& packet)
    {
        packet = {};
        packet.entity = entity;
        packet.component = component;
        packet.type = input::ComponentEventType::Add;

        return true;
    }
};

template <typename TEventStream>
    requires ReliableEventStreamLike<TEventStream>
class ReliableEventNetInputStream
{
   public:
    using EventStream = TEventStream;
    using Event = typename EventStream::Event;
    using Packet = Event;

   private:
    EventStream& m_stream;

   public:
    explicit ReliableEventNetInputStream(entt::registry& ctx)
        : m_stream(ctx.ctx().template get<EventStream>())
    {
    }

    explicit ReliableEventNetInputStream(EventStream& stream) : m_stream(stream)
    {
    }

    ReliableEventNetInputStream(const ReliableEventNetInputStream&) = delete;

    ReliableEventNetInputStream& operator=(const ReliableEventNetInputStream&) =
        delete;

    ReliableEventNetInputStream(ReliableEventNetInputStream&&) = delete;

    ReliableEventNetInputStream& operator=(ReliableEventNetInputStream&&) =
        delete;

    bool Insert(const DecodeContext& dctx, const Packet& packet)
    {
        (void)dctx;

        /*
            Sequenced delivery assumed.

            No reordering, receive cursor, or tick buffering is needed.
            The received event is appended to the local event stream.
        */
        m_stream.Push(packet);
        return true;
    }
};
}  // namespace game::net