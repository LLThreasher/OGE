#pragma once

#include <array>
#include <concepts>
#include <cstdint>
#include <tuple>
#include <vector>

#include "game/components.hpp"
#include "game/components_net.hpp"
#include "game/input/entity_event_stream.hpp"
#include "game/input/net.hpp"
#include "game/terrain/terrain_view.hpp"
#include "oge/event_stream.hpp"
#include "oge/log.hpp"
#include "oge/runtime/net_packet_sender.hpp"
#include "oge/runtime/net_serializer.hpp"
#include "oge/runtime/net_server.hpp"
#include "oge/runtime/net_traits.hpp"
#include "oge/runtime/typed_registry.hpp"

namespace game
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

template <typename TEvent>
using NetEventBatch = std::pmr::vector<TEvent>;

struct ReplicationCapability : ICapability
{
    using EncodeFn = void (*)(entt::registry&, ENetPeer*,
                              oge::runtime::NetPacketSender&, FamilyId,
                              SendType, uint8_t, entt::any&);

    using DecodeFn = void (*)(entt::registry&, net::Buffer&);

    using CreateStateFn = entt::any (*)();  // returns initialized state

    FamilyId family;
    SendType sendType = SendType::Reliable;
    uint8_t channel = 0;

    EncodeFn encode = nullptr;
    DecodeFn decode = nullptr;
    CreateStateFn createState = nullptr;

    ReplicationCapability(FamilyId f, EncodeFn e, DecodeFn d, CreateStateFn cs,
                          SendType st = SendType::Reliable, uint8_t ch = 0)
        : family(f),
          sendType(st),
          channel(ch),
          encode(e),
          decode(d),
          createState(cs)
    {
    }
};

class ReplicationRegistry
{
    struct PeerState
    {
        std::unordered_map<FamilyId, entt::any> state;
    };

    std::vector<oge_id_type> m_serializableComponents;
    std::vector<FamilyId> m_sendUnits;
    std::unordered_map<FamilyId, ReplicationCapability*> m_units;
    std::unordered_map<ENetPeer*, PeerState> m_peers;

   public:
    const std::vector<oge_id_type>& SeralizableComponents() const
    {
        return m_serializableComponents;
    }

    template <typename T>
    void RegisterSerializableComponent()
    {
        m_serializableComponents.push_back(entt::type_hash<T>::value());
    }

    void AddFamilyToSend(FamilyId id)
    {
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
                    entt::registry& world)
    {
        for (auto& [peer, peerState] : m_peers)
        {
            for (auto family : m_sendUnits)
            {
                auto cap = m_units.at(family);
                cap->encode(world, peer, server, family, cap->sendType,
                            cap->channel, peerState.state.at(family));
            }
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

    auto Peers() const
    {
        return m_peers;
    }
};

void InstallEntityReplicationHooks(entt::registry& world);

struct EntityReplication
{
    struct State
    {
        typename input::EntityEventStream::Cursor cursor = 0;
        bool useSnapshot = true;
    };

    static entt::any CreateState()
    {
        return State{};
    }

    static void Encode(entt::registry& world, ENetPeer* peer,
                       oge::runtime::NetPacketSender& server, FamilyId family,
                       SendType sendType, uint8_t channel, entt::any& anyState);

    static void Decode(entt::registry& world, net::Buffer& buffer);
};

template <typename T>
void InstallComponentReplicationHooks(entt::registry& world)
{
    world.ctx().emplace<input::ComponentDeltaStream<T>>();
    world.on_construct<T>()
        .template connect<
            +[](entt::registry& world, entt::entity e)
            {
                world.ctx().template get<input::ComponentDeltaStream<T>>().Push(
                    {input::ComponentDeltaType::Add, e});
            }>();
    world.on_update<T>()
        .template connect<
            +[](entt::registry& world, entt::entity e)
            {
                world.ctx().template get<input::ComponentDeltaStream<T>>().Push(
                    {input::ComponentDeltaType::Update, e});
            }>();
    world.on_destroy<T>()
        .template connect<
            +[](entt::registry& world, entt::entity e)
            {
                world.ctx().template get<input::ComponentDeltaStream<T>>().Push(
                    {input::ComponentDeltaType::Remove, e});
            }>();
}

template <typename T>
struct ComponentReplication
{
    struct State
    {
        typename input::ComponentDeltaStream<T>::Cursor cursor{};
        bool needsSnapshot = true;
    };

    static void SendSnapshot(entt::registry& world, ENetPeer* peer,
                             oge::runtime::NetPacketSender& server,
                             FamilyId family, SendType sendType,
                             uint8_t channel, State& state)
    {
        auto view = world.view<T>();

        for (auto entity : view)
        {
            size_t size = sizeof(FamilyId) + sizeof(input::ComponentDeltaType) +
                          sizeof(entt::entity) +
                          net::Size(world.get<T>(entity));

            auto packet = server.StartPacket(size);

            packet.Write(family);
            packet.Write(input::ComponentDeltaType::Add);
            packet.Write(entity);

            net::Serialize(packet, world.get<T>(entity));

            server.Send(peer, packet, sendType, channel);
        }

        state.needsSnapshot = false;
    }

    static entt::any CreateState()
    {
        return State{};
    }

    static void Encode(entt::registry& world, ENetPeer* peer,
                       oge::runtime::NetPacketSender& server, FamilyId family,
                       SendType sendType, uint8_t channel, entt::any& anyState)
    {
        // LOG_DEBUG("encode {}", oge::runtime::TypeName<T>::Get());
        auto& state = entt::any_cast<State&>(anyState);

        auto* stream = world.ctx().find<input::ComponentDeltaStream<T>>();
        if (!stream) return;

        if (state.needsSnapshot)
        {
            stream->AdvanceCursor(state.cursor);
            SendSnapshot(world, peer, server, family, sendType, channel, state);
            return;
        }

        entt::sparse_set newComps;
        entt::storage<T> storage;
        input::ComponentDeltaEvent<T> delta;
        while (stream->PollOne(state.cursor, delta))
        {
            if (!world.valid(delta.entity)) continue;
            if (delta.type != input::ComponentDeltaType::Remove)
            {
                if (!storage.contains(delta.entity))
                    storage.emplace(delta.entity, world.get<T>(delta.entity));
                if (delta.type == input::ComponentDeltaType::Add)
                    newComps.push(delta.entity);
            }
            else
            {
                if (storage.contains(delta.entity))
                {
                    storage.erase(delta.entity);
                }
                if (newComps.contains(delta.entity))
                {
                    newComps.erase(delta.entity);
                }
                else
                {
                    size_t size = sizeof(FamilyId) +
                                  sizeof(input::ComponentDeltaType) +
                                  sizeof(entt::entity);
                    auto packet = server.StartPacket(size);
                    packet.Write(family);
                    packet.Write(delta.type);
                    packet.Write(delta.entity);
                    server.Send(peer, packet, sendType, channel);
                }
            }
        }

        for (auto [e, data] : storage.each())
        {
            auto ty = newComps.contains(e) ? input::ComponentDeltaType::Add
                                           : input::ComponentDeltaType::Update;
            size_t size = sizeof(FamilyId) + sizeof(input::ComponentDeltaType) +
                          sizeof(entt::entity) +
                          net::Size(world.get<T>(delta.entity));

            auto packet = server.StartPacket(size);

            packet.Write(family);
            packet.Write(delta.type);
            packet.Write(delta.entity);
            net::Serialize(packet, world.get<T>(delta.entity));
            server.Send(peer, packet, sendType, channel);
        }
    }

    static void Decode(entt::registry& world, net::Buffer& buffer)
    {
        input::ComponentDeltaType type;
        buffer.Read(type);

        entt::entity entity;
        buffer.Read(entity);

        // LOG_DEBUG("decode {} for {}", oge::runtime::TypeName<T>::Get(),
        // (uint64_t)entity);

        switch (type)
        {
            case input::ComponentDeltaType::Add:
            case input::ComponentDeltaType::Update:
            {
                if (!world.all_of<T>(entity))
                {
                    T res{};
                    net::Deserialize(buffer, res);
                    world.emplace<T>(entity, std::move(res));  // this line
                }
                else
                {
                    T& res = world.get<T>(entity);
                    net::Deserialize(buffer, res);
                    world.patch<T>(entity);
                }
                break;
            }

            case input::ComponentDeltaType::Remove:
            {
                if (world.valid(entity) && world.all_of<T>(entity))
                    world.remove<T>(entity);
                break;
            }
        }
    }
};

void InstallTerrainReplicationHooks(entt::registry& world);

struct TerrainReplication
{
    struct State
    {
        terrain::ChunkHandle snapshotCursor{};
        bool needsSnapshot = true;
        terrain::ChunkEventStream::Cursor chunkEventCursor{};
    };

    static entt::any CreateState();
    static void Encode(entt::registry& world, ENetPeer* peer,
                       oge::runtime::NetPacketSender& server, FamilyId family,
                       SendType sendType, uint8_t channel, entt::any& anyState);
    static void Decode(entt::registry& world, net::Buffer& buffer);
};

template <typename T>
concept IsEventStream = requires(T s, T::Cursor c, T::TEvent e) {
    typename T::TEvent;
    typename T::Cursor;
    { s.AdvanceCursor(c) };
    { s.PollOne(c, e) } -> std::same_as<bool>;
    { s.Push(e) } -> std::same_as<void>;
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
                       oge::runtime::NetPacketSender& server, FamilyId family,
                       SendType sendType, uint8_t channel, entt::any& anyState)
    {
        auto& state = entt::any_cast<State&>(anyState);
        auto& stream = world.ctx().get<TEventStream>();

        if (!state.initialized)
        {
            stream.AdvanceCursor(state.cursor);
            state.initialized = true;
            return;
        }

        NetEventBatch<TEvent> batch;

        TEvent ev;
        while (stream.PollOne(state.cursor, ev))
            batch.events.Add(std::move(ev));

        if (batch.events.empty()) return;

        auto packet = server.StartPacket(sizeof(FamilyId) + net::Size(batch));

        packet.Write(family);
        net::Serialize(packet, batch);

        server.Send(peer, packet, sendType, channel);
    }

    static void Decode(entt::registry& world, net::Buffer& buffer)
    {
        NetEventBatch<TEvent> batch;
        net::Deserialize(buffer, batch);

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
        EntityEventStream::Cursor eCursor;
    };

    static entt::any CreateState()
    {
        LOG_DEBUG("create e event state");
        return State{};
    }

    static void Encode(entt::registry& world, ENetPeer* peer,
                       oge::runtime::NetPacketSender& server, FamilyId family,
                       SendType sendType, uint8_t channel, entt::any& anyState)
    {
        auto& state = entt::any_cast<State&>(anyState);

        EntityEvent ee;
        auto eStream = world.ctx().find<EntityEventStream>();
        if (!eStream) return;
        while (eStream->PollOne(state.eCursor, ee))
        {
            if (ee.type == EntityEventType::Destroy)
            {
                state.perStreamStates.erase(ee.entity);
            }
        }

        auto view = world.view<TEventStream>();

        for (auto entity : view)
        {
            auto& stream = world.get<TEventStream>(entity);

            PerStreamState& pState = state.perStreamStates[entity];

            if (!pState.initialized)
            {
                stream.AdvanceCursor(pState.cursor);
                pState.initialized = true;
                continue;
            }

            NetEventBatch<TEvent> batch;

            TEvent ev;
            while (stream.PollOne(pState.cursor, ev))
                batch.push_back(std::move(ev));

            if (batch.empty()) continue;

            auto packet = server.StartPacket(
                sizeof(FamilyId) + sizeof(entt::entity) + net::Size(batch));

            packet.Write(family);
            packet.Write(entity);
            net::Serialize(packet, batch);

            server.Send(peer, packet, sendType, channel);
        }
    }

    static void Decode(entt::registry& world, net::Buffer& buffer)
    {
        auto entity = buffer.Read<entt::entity>();

        NetEventBatch<TEvent> batch;
        net::Deserialize(buffer, batch);

        auto& queue = world.get<TEventStream>(entity);

        for (auto& e : batch) queue.Push(std::move(e));
    }
};

void RegisterReplications(oge::runtime::AnythingFactory& af,
                          ReplicationRegistry& rf);
}  // namespace game
