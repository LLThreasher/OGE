#pragma once

#include <concepts>
#include <type_traits>

#include "oge/event_stream.hpp"
#include "oge/runtime/net_packet_sender.hpp"
#include "oge/runtime/net_serializer.hpp"
#include "oge/runtime/net_server.hpp"
#include "oge/runtime/typed_registry.hpp"

namespace game
{
using oge::runtime::FamilyId;
using oge::runtime::ICapability;
using oge::runtime::oge_id_type;
using oge::runtime::SendType;
using oge::runtime::TypeRegistry;
namespace net = oge::runtime::net;
using oge::DiscreteEventStream;

template <typename T>
struct EntityStorageTraits
{
    static size_t GetSize(entt::entity entity, entt::registry& world)
    {
        auto& comp = world.get<T>(entity);
        return comp.Size();
    }

    static void Serialize(entt::entity entity, entt::registry& world,
                          net::Buffer& buffer)
    {
        buffer.Write(entity);

        auto& comp = world.get<T>(entity);
        comp.Serialize(buffer);  // explicit contract
    }

    static void Deserialize(entt::registry& world, net::Buffer& buffer)
    {
        entt::entity entity = buffer.Read<entt::entity>();

        T comp;
        comp.Deserialize(buffer);

        world.emplace_or_replace<T>(entity, std::move(comp));
    }
};

template <typename TEvent>
struct NetEventBatch : net::Object<NetEventBatch<TEvent>>
{
    net::List<TEvent> events;

    NET_OBJ_FN
    {
        visit(events);
    }
};

struct ReplicationCapability : ICapability
{
    using EncodeFn = void (*)(entt::registry&, ENetPeer*,
                              oge::runtime::NetServer&, FamilyId, SendType,
                              uint8_t, entt::any&);

    using DecodeFn = void (*)(entt::registry&, net::Buffer&);

    using CreateStateFn = entt::any (*)();  // returns initialized state

    FamilyId family;
    SendType sendType = SendType::Reliable;
    uint8_t channel = 0;

    EncodeFn encode = nullptr;
    DecodeFn decode = nullptr;
    CreateStateFn createState = nullptr;

    ReplicationCapability(FamilyId f, EncodeFn e, DecodeFn d,
                          SendType st = SendType::Reliable, uint8_t ch = 0,
                          CreateStateFn cs = nullptr)
        : family(f),
          sendType(st),
          channel(ch),
          encode(e),
          decode(d),
          createState(cs)
    {
    }

    template <typename Impl>
    static ReplicationCapability Create(FamilyId family,
                                        SendType sendType = SendType::Reliable,
                                        uint8_t channel = 0)
    {
        return {family, &Impl::Encode,     &Impl::Decode, SendType::Reliable,
                0,      &Impl::CreateState};
    }
};

template <typename T>
struct ComponentReplication
{
    struct State
    {
        std::unordered_map<entt::entity, uint32_t> lastSeen;
    };

    static entt::any CreateState()
    {
        return State{};
    }

    static void Encode(entt::registry& world, ENetPeer* peer,
                       oge::runtime::NetServer& server, FamilyId family,
                       SendType sendType, uint8_t channel, entt::any& anyState)
    {
        auto& state = entt::any_cast<State&>(anyState);
        auto& storage = world.storage<T>();

        for (auto entity : storage)
        {
            uint32_t current = storage.current(entity);
            uint32_t& last = state.lastSeen[entity];

            if (current <= last) continue;

            last = current;

            size_t size = EntityStorageTraits<T>::GetSize(entity, world);

            auto packet = server.StartPacket(sizeof(FamilyId) + size);

            packet.Write(family);

            EntityStorageTraits<T>::Serialize(entity, world, packet);

            server.Send(peer, packet, sendType, channel);
        }
    }

    static void Decode(entt::registry& world, net::Buffer& buffer)
    {
        EntityStorageTraits<T>::Deserialize(world, buffer);
    }
};

template <typename T>
concept IsEventStream = requires(T s, T::Cursor c, T::Event e) {
    typename T::TEvent;
    typename T::Cursor;
    { s.HeadIndex() } -> std::same_as<typename T::Cursor>;
    { s.PollOne(c, e) } -> std::same_as<bool>;
    { s.Push(c, e) } -> std::same_as<void>;
};

template <typename TEventStream>
    requires IsEventStream<TEventStream>
struct EventStreamReplication
{
    using TEvent = TEventStream::TEvent;
    struct State
    {
        typename TEventStream::Cursor cursor = 0;
        bool initialized = false;
    };

    static entt::any CreateState()
    {
        return State{};
    }

    static void Encode(entt::registry& world, ENetPeer* peer,
                       oge::runtime::NetServer& server, FamilyId family,
                       SendType sendType, uint8_t channel, entt::any& anyState)
    {
        auto& state = entt::any_cast<State&>(anyState);
        auto& stream = world.ctx().get<TEventStream>();

        if (!state.initialized)
        {
            state.cursor = stream.HeadIndex();
            state.initialized = true;
            return;
        }

        NetEventBatch<TEvent> batch;

        TEvent ev;
        while (stream.PollOne(state.cursor, ev))
            batch.events.Add(std::move(ev));

        if (batch.events.empty()) return;

        auto packet = server.StartPacket(sizeof(FamilyId) + batch.Size());

        packet.Write(family);
        batch.Serialize(packet);

        server.Send(peer, packet, sendType, channel);
    }

    static void Decode(entt::registry& world, net::Buffer& buffer)
    {
        NetEventBatch<TEvent> batch;
        batch.Deserialize(buffer);

        auto& queue = world.ctx().get<TEventStream>();

        for (auto& e : batch.events) queue.Push(std::move(e));
    }
};

template <typename TEventStream>
    requires IsEventStream<TEventStream>
struct EntityEventStreamReplication
{
    using TEvent = typename TEventStream::TEvent;

    struct PerStreamState
    {
        typename TEventStream::Cursor cursor{};
        bool initialized = false;
    };

    struct State
    {
        std::unordered_map<entt::entity, PerStreamState> perStreamStates;
    };

    static entt::any CreateState()
    {
        return State{};
    }

    static void Encode(entt::registry& world, ENetPeer* peer,
                       oge::runtime::NetServer& server, FamilyId family,
                       SendType sendType, uint8_t channel, entt::any& anyState)
    {
        auto& state = entt::any_cast<State&>(anyState);

        auto view = world.view<TEventStream>();

        for (auto entity : view)
        {
            auto& stream = world.get<TEventStream>(entity);

            PerStreamState& pState = state.perStreamStates[entity];

            if (!pState.initialized)
            {
                pState.cursor = stream.HeadIndex();
                pState.initialized = true;
                continue;
            }

            NetEventBatch<TEvent> batch;

            TEvent ev;
            while (stream.PollOne(pState.cursor, ev))
                batch.events.Add(std::move(ev));

            if (batch.events.empty()) continue;

            auto packet = server.StartPacket(
                sizeof(FamilyId) + sizeof(entt::entity) + batch.Size());

            packet.Write(family);
            packet.Write(entity);
            batch.Serialize(packet);

            server.Send(peer, packet, sendType, channel);
        }
    }

    static void Decode(entt::registry& world, net::Buffer& buffer)
    {
        auto entity = buffer.Read<entt::entity>();

        NetEventBatch<TEvent> batch;
        batch.Deserialize(buffer);

        auto& queue = world.get<TEventStream>(entity);

        for (auto& e : batch.events) queue.Push(std::move(e));
    }
};

class ReplicationRegistry
{
    struct PeerState
    {
        std::unordered_map<FamilyId, entt::any> state;
    };

    std::unordered_map<FamilyId, ReplicationCapability*> m_units;
    std::unordered_map<ENetPeer*, PeerState> m_peers;

   public:
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

    void ProduceAll(oge::runtime::NetServer& server, ENetPeer* peer,
                    entt::registry& world)
    {
        auto it = m_peers.find(peer);
        if (it == m_peers.end()) return;

        auto& peerState = it->second;

        for (auto& [family, cap] : m_units)
        {
            entt::any& state = peerState.state[family];

            cap->encode(world, peer, server, family, cap->sendType,
                        cap->channel, state);
        }
    }

    void HandleIncoming(entt::registry& world, net::Buffer& buffer)
    {
        FamilyId family = buffer.Read<FamilyId>();

        auto it = m_units.find(family);
        if (it == m_units.end()) return;

        it->second->decode(world, buffer);
    }

    void AddPeer(ENetPeer* peer)
    {
        PeerState peerState;

        for (auto& [family, cap] : m_units)
        {
            if (cap->createState)
            {
                peerState.state.emplace(family, cap->createState());
            }
        }

        m_peers.emplace(peer, std::move(peerState));
    }

    void RemovePeer(ENetPeer* peer)
    {
        m_peers.erase(peer);  // entt::any cleans itself
    }
};
}  // namespace game
