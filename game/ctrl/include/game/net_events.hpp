#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <unordered_map>
#include <vector>

#include "entt/entity/fwd.hpp"
#include "entt/signal/fwd.hpp"
#include "game/components.hpp"
#include "game/input/player_input_stream.hpp"
#include "game/terrain/terrain_view.hpp"
#include "oge/event_stream.hpp"
#include "oge/math.hpp"
#include "oge/runtime/entt.hpp"
#include "oge/runtime/net_client.hpp"
#include "oge/runtime/net_packet_sender.hpp"
#include "oge/runtime/net_serializer.hpp"
#include "oge/runtime/net_server.hpp"
#include "oge/runtime/typed_registry.hpp"

namespace game
{
namespace net = oge::runtime::net;
struct ClientNetEvent
{
    uint8_t id;
    std::span<std::byte> data;

    bool IsChunkStreaming() const
    {
        return id == 1;
    }

    bool IsDelta() const
    {
        return id > 1 && ((id & 128) == 0);
    }

    bool IsRPC() const
    {
        return (id & 128) != 0;
    }
};

struct ServerNetEvent
{
    uint8_t id;
    std::span<std::byte> data;
};

namespace events
{
namespace server
{
struct PlayerInputEvent
{
    static constexpr uint8_t Id = 1;
    input::PlayerInputEvent event;

    void Serialize(oge::runtime::net::Buffer& buffer)
    {
        buffer.Write<uint8_t>(event.actionMask);
        buffer.Write<math::vec2>(event.actionPos);
    }

    void Deserialize(oge::runtime::net::Buffer& buffer)
    {
        event.actionMask = buffer.Read<uint8_t>();
        event.actionPos = buffer.Read<math::vec2>();
    }
};

NET_OBJ(PlayerMoveEvent)
{
    static constexpr uint8_t Id = 2;
    net::Vec2 move;

    NET_OBJ_FN
    {
        visit(move);
    }
};

NET_OBJ(PlayerPanEvent)
{
    static constexpr uint8_t Id = 3;
    net::Vec2 pan;

    NET_OBJ_FN
    {
        visit(pan);
    }
};

using oge::runtime::OnServerReceiveConnect;
using oge::runtime::OnServerReceiveDisconnect;
using oge::runtime::OnServerReceivePacket;
}  // namespace server

namespace client
{

struct SendChunkEvent
{
    static constexpr uint8_t Id = 1;
    terrain::PaletteCompressedChunk chunk;

    void Serialize(net::Buffer& buffer)
    {
        buffer.Write(chunk.palette.size());
        buffer.Write(std::as_bytes(std::span<uint32_t>(chunk.palette)));
        buffer.Write(chunk.data);
    }

    void Deserialize(net::Buffer& buffer)
    {
        chunk.palette.resize(buffer.Read<size_t>());
        buffer.ReadRaw(chunk.palette.data(),
                       chunk.palette.size() * sizeof(uint32_t));
        buffer.ReadRaw(chunk.data, terrain::CHUNK_SIZE_TOTAL);
    }

    void Apply(entt::dispatcher& dispatcher)
    {
        dispatcher.trigger(*this);
    }
};

inline bool IsChunkStreaming(uint8_t id)
{
    return id == SendChunkEvent::Id;
}
}  // namespace client

template <typename T>
struct EntityStorageTraits
{
    static void Deserialize(entt::registry& world, net::Buffer& buffer)
    {
        auto entity = buffer.Read<entt::entity>();
        world.get<T>(entity).Deserialize(buffer);
    }

    static void Serialize(entt::entity entity, entt::registry& world,
                          net::Buffer& buffer)
    {
        buffer.Write(entity);
        world.get<T>(entity).Serialize(buffer);
    }

    static size_t GetSize(entt::entity entity, entt::registry& world)
    {
        return world.get<T>(entity).NetSize();
    }
};

template <typename T>
struct GlobalStorageTraits
{
    static void Deserialize(entt::registry& world, net::Buffer& buffer)
    {
        world.ctx().get<T>().Deserialize(buffer);
    }

    static void Serialize(entt::registry& world, net::Buffer& buffer)
    {
        world.ctx().get<T>().Serialize(buffer);
    }

    static size_t GetSize(entt::registry& world)
    {
        return world.ctx().get<T>().NetSize();
    }
};

template <typename TComponent, typename TStorageTraits>
struct DeltaPacketHandler : private TStorageTraits
{
    using TStorageTraits::Deserialize;
    using TStorageTraits::GetSize;
    using TStorageTraits::Serialize;
};

template <typename T>
using EntityDeltaPacketHandler = DeltaPacketHandler<T, EntityStorageTraits<T>>;

template <typename T>
using GlobalDeltaPacketHandler = DeltaPacketHandler<T, GlobalStorageTraits<T>>;

class IncomingEventRegistry
{
    std::vector<void (*)(entt::registry&, net::Buffer&)>
        m_componentHandlersIncoming;
    uint8_t m_nextEntityComponentId;
    uint8_t m_nextGlobalComponentId;

   public:
    IncomingEventRegistry(uint8_t entityStart = 2, uint8_t globalStart = 128)
    {
        m_componentHandlersIncoming.resize(256);
    }

    template <typename TComponent>
    void RegisterNetComponent()
    {
        m_componentHandlersIncoming[m_nextEntityComponentId++] =
            EntityStorageTraits<TComponent>::Deserialize;
    }

    template <typename TComponent>
    void RegisterGlobalComponent()
    {
        m_componentHandlersIncoming[m_nextGlobalComponentId++] =
            GlobalStorageTraits<TComponent>::Deserialize;
    }

    void HandleIncomingPacket(uint8_t packetId, entt::registry& world,
                              net::Buffer& buffer)
    {
        auto& handler = m_componentHandlersIncoming[packetId];
        assert(handler);
        handler(world, buffer);
    }
};

class ClientNetObjectTransportLayer
{
    events::IncomingEventRegistry m_incomingDeltaEventRegistry;
    entt::registry& m_world;
    entt::dispatcher& m_clientDispatcher;
    entt::dispatcher& m_worldEventDispatcher;
    input::PlayerInputStream* m_playerInput = nullptr;

    void onClientReceivePacket(oge::runtime::OnClientReceivePacket ctx)
    {
        uint8_t packetId = ctx.data.Read<uint8_t>();
        m_incomingDeltaEventRegistry.HandleIncomingPacket(packetId, m_world,
                                                          ctx.data);
    }

    void onClientConnected(oge::runtime::OnClientConnected ctx)
    {
    }

    void onClientConnectionTimeout(oge::runtime::OnClientConnectionTimeout ctx)
    {
    }

    void onClientDisconnected(oge::runtime::OnClientDisconnected ctx)
    {
    }

   public:
    ClientNetObjectTransportLayer(entt::registry& world,
                                  entt::dispatcher& clientDispatcher,
                                  entt::dispatcher& worldDispatcher)
        : m_world(world),
          m_clientDispatcher(clientDispatcher),
          m_worldEventDispatcher(worldDispatcher)
    {
        m_clientDispatcher.sink<oge::runtime::OnClientReceivePacket>()
            .connect<&ClientNetObjectTransportLayer::onClientReceivePacket>(
                this);
        m_clientDispatcher.sink<oge::runtime::OnClientConnected>()
            .connect<&ClientNetObjectTransportLayer::onClientConnected>(this);
        m_clientDispatcher.sink<oge::runtime::OnClientConnectionTimeout>()
            .connect<&ClientNetObjectTransportLayer::onClientConnectionTimeout>(
                this);
        m_clientDispatcher.sink<oge::runtime::OnClientDisconnected>()
            .connect<&ClientNetObjectTransportLayer::onClientDisconnected>(
                this);
    }

    ~ClientNetObjectTransportLayer()
    {
        m_clientDispatcher.disconnect(this);
    }

    template <typename TComponent>
    void RegisterNetComponent()
    {
        m_incomingDeltaEventRegistry.RegisterNetComponent<TComponent>();
    }

    void SetPlayerInput(input::PlayerInputStream* in)
    {
        m_playerInput = in;
    }

    void PostUpdate()
    {
        if (m_playerInput)
        {
        }
    }
};

using oge::runtime::SendType;

class OutgoingDeltaEventRegistry
{
    struct Entry
    {
        void (*packetBuilder)(entt::registry&, entt::entity, net::Buffer&);
        size_t (*sizeGetter)(entt::registry&, entt::entity);
        uint8_t channel;
        SendType sendType;
        bool isGlobal;
    };

    std::vector<Entry> m_componentHandlersOutgoing;
    std::array<uint8_t, 4> channelToNextComponentId = {};
    uint8_t m_entityBase;
    uint8_t m_gloablBase;
    uint8_t m_nextEntityId = m_entityBase;
    uint8_t m_nextGlobalId = m_gloablBase;

   public:
    OutgoingDeltaEventRegistry(uint8_t entityStart = 2, uint8_t globalStart = 128)
        : m_entityBase(entityStart), m_gloablBase(globalStart)
    {
    }

    template <typename TComponent>
    void RegisterNetComponent(SendType sendType = SendType::Reliable,
                              size_t channel = 0)
    {
        m_componentHandlersOutgoing.push_back(
            {.packetBuilder =
                 [](entt::registry& world, entt::entity entity,
                    net::Buffer& buffer)
             {
                 EntityStorageTraits<TComponent>::Serialize(entity, world,
                                                            buffer);
             },
             .sizeGetter =
                 [](entt::registry& world, entt::entity entity)
             {
                 return EntityStorageTraits<TComponent>::GetSize(entity, world);
             },
             .channel = channel,
             .sendType = sendType,
             .isGlobal = false});

        channelToNextComponentId[channel]++;
    }

    template <typename TComponent>
    void RegisterGlobalComponent(SendType sendType = SendType::Reliable,
                                 size_t channel = 0)
    {
        m_componentHandlersOutgoing.push_back(
            {.packetBuilder =
                 [](entt::registry& world, entt::entity, net::Buffer& buffer)
             { GlobalStorageTraits<TComponent>::Serialize(world, buffer); },
             .sizeGetter = [](entt::registry& world, entt::entity)
             { return GlobalStorageTraits<TComponent>::GetSize(world); },
             .channel = channel,
             .sendType = sendType,
             .isGlobal = true});

        channelToNextComponentId[channel]++;
    }

    void ProducePackets(oge::runtime::NetServer& server, ENetPeer* peer,
                        entt::registry& world)
    {
        // ===== Entity components =====
        for (auto [entity, dirty] : world.view<const DirtyTag>().each())
        {
            for (uint8_t id : dirty.dirtyComponents)
            {
                auto& handler = m_componentHandlersOutgoing[id];

                if (handler.isGlobal) continue;

                auto size = handler.sizeGetter(world, entity);
                auto packet = server.StartPacket(size);

                handler.packetBuilder(world, entity, packet);
                server.Send(peer, packet, handler.sendType, handler.channel);
            }
        }

        // ===== Global components =====
        if (auto globalDirty = world.ctx().find<const DirtyTag>())
        {
            for (uint8_t id : globalDirty->dirtyComponents)
            {
                auto& handler = m_componentHandlersOutgoing[id];

                if (!handler.isGlobal) continue;

                // You probably want a global dirty flag check here
                // Example:
                // if (!IsGlobalDirty(id)) continue;

                auto size = handler.sizeGetter(world, entt::null);
                auto packet = server.StartPacket(size);
                packet.Write(id + m_gloablBase);
                handler.packetBuilder(world, entt::null, packet);
                server.Send(peer, packet, handler.sendType, handler.channel);
            }
        }
    }
};

class ServerNetObjectTransportationLayer
{
    enum class ClientState : uint32_t
    {
        Connected = 0,
        HasPlayer,
        Disconnected,
    };

    struct ClientInstance
    {
        ClientState state;
        ENetPeer* peer;
        input::PlayerInputStream* playerInputStream;
    };

    entt::registry& m_world;
    entt::dispatcher& m_dispatcher;
    std::unordered_map<uint32_t, ClientInstance> m_clients;
    std::vector<entt::connection> m_dispatcherConnections;

    events::OutgoingDeltaEventRegistry m_outgoingDeltaEventRegistry;

    void onServerReciveConnect(server::OnServerReceiveConnect ctx)
    {
        m_clients.emplace(ctx.peerId, ClientInstance{});
    }

    void onServerReciveDisconnect(server::OnServerReceiveDisconnect ctx)
    {
        m_clients.erase(ctx.peerId);
    }

    void onServerRecivePacket(server::OnServerReceivePacket ctx)
    {
        auto& client = m_clients[ctx.peerId];
        uint8_t packetId = ctx.data.Read<uint8_t>();
        switch (packetId)
        {
            case server::PlayerInputEvent::Id:
                client.playerInputStream->InsertAction(
                    ctx.data.ReadAndDeserialize<server::PlayerInputEvent>()
                        .event);
                break;
            case server::PlayerMoveEvent::Id:
                client.playerInputStream->InsertMoveDelta(
                    ctx.data.ReadAndDeserialize<server::PlayerMoveEvent>()
                        .move);
                break;
            case server::PlayerPanEvent::Id:
                client.playerInputStream->InsertPanDelta(
                    ctx.data.ReadAndDeserialize<server::PlayerPanEvent>().pan);
                break;
        }
    }

   public:
    ServerNetObjectTransportationLayer(entt::registry& world,
                                       entt::dispatcher& serverDispatcher)
        : m_world(world), m_dispatcher(serverDispatcher)
    {
        m_dispatcher.sink<server::OnServerReceiveConnect>()
            .connect<
                &ServerNetObjectTransportationLayer::onServerReciveConnect>(
                this);
        m_dispatcher.sink<server::OnServerReceiveDisconnect>()
            .connect<
                &ServerNetObjectTransportationLayer::onServerReciveDisconnect>(
                this);
        m_dispatcher.sink<server::OnServerReceivePacket>()
            .connect<&ServerNetObjectTransportationLayer::onServerRecivePacket>(
                this);
    }

    ~ServerNetObjectTransportationLayer()
    {
        m_dispatcher.disconnect(this);
    }

    template <typename TComponent>
    void RegisterNetComponent()
    {
        m_outgoingDeltaEventRegistry.RegisterNetComponent<TComponent>();
    }

    void PostUpdate(oge::runtime::NetServer& server, entt::registry& world)
    {
        auto& terrain = world.ctx().get<terrain::TerrainView>();
        for (const auto& instance : m_clients)
        {
            m_outgoingDeltaEventRegistry.ProducePackets(
                server, instance.second.peer, world);
        }
    }
};
}  // namespace events
}  // namespace game
