#pragma once

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

    bool IsChunkStreaming() const { return id == 1; }

    bool IsDelta() const { return id > 1 && ((id & 128) == 0); }

    bool IsRPC() const { return (id & 128) != 0; }
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

    NET_OBJ_FN { visit(move); }
};

NET_OBJ(PlayerPanEvent)
{
    static constexpr uint8_t Id = 3;
    net::Vec2 pan;

    NET_OBJ_FN { visit(pan); }
};

using oge::runtime::OnServerReceiveConnect;
using oge::runtime::OnServerReceiveDisconnect;
using oge::runtime::OnServerReceivePacket;
}  // namespace server

namespace client
{
constexpr uint8_t DELTA_BASE = 2;
constexpr uint8_t RPC_BASE = 128;

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

    void Apply(entt::dispatcher& dispatcher) { dispatcher.trigger(*this); }
};

NET_OBJ(AddPlayerEvent)
{
    static constexpr uint8_t Id = RPC_BASE;
    net::Vec3 pos;

    NET_OBJ_FN { visit(pos); }

    void Apply(entt::dispatcher & dispatcher) { dispatcher.trigger(*this); }
};

inline bool IsChunkStreaming(uint8_t id) { return id == SendChunkEvent::Id; }

inline bool IsDelta(uint8_t id) { return id >= DELTA_BASE && id < RPC_BASE; }

inline bool IsRPC(uint8_t id) { return id >= RPC_BASE; }

template <typename TComponent>
struct DeltaPacketHandler
{
    static void Deserialize(entt::registry& world, net::Buffer& buffer)
    {
        auto entity = buffer.Read<entt::entity>();
        world.get<TComponent>(entity).Deserialize(buffer);
    }

    static void Serialize(entt::entity entity, entt::registry& world,
                          net::Buffer& buffer)
    {
        buffer.Write(entity);
        world.get<TComponent>(entity).Serialize(buffer);
    }

    static size_t GetSize() { return sizeof(TComponent); }
};

using oge::runtime::oge_id_type;

class ServerNetTransportationLayer
{
   public:
};
}  // namespace client

class IncomingDeltaEventRegistry
{
    std::vector<void (*)(entt::registry&, net::Buffer&)>
        m_componentHandlersIncoming;

   public:
    template <typename TComponent>
    void RegisterNetComponent()
    {
        m_componentHandlersIncoming.push_back(
            client::DeltaPacketHandler<TComponent>::Deserialize);
    }

    void HandleIncomingPacket(uint8_t packetId, entt::registry& world,
                              net::Buffer& buffer)
    {
        assert(packetId - client::DELTA_BASE <
               m_componentHandlersIncoming.size());
        m_componentHandlersIncoming[packetId - client::DELTA_BASE](world,
                                                                   buffer);
    }
};

class ClientNetTransportLayer
{
    events::IncomingDeltaEventRegistry m_incomingDeltaEventRegistry;
    entt::registry& m_world;
    entt::dispatcher& m_clientDispatcher;
    entt::dispatcher& m_worldEventDispatcher;
    input::PlayerInputStream* m_playerInput = nullptr;

    void onClientReceivePacket(oge::runtime::OnClientReceivePacket ctx)
    {
        uint8_t packetId = ctx.data.Read<uint8_t>();
        if (client::IsChunkStreaming(packetId))
        {
            m_worldEventDispatcher.trigger(
                ctx.data.ReadAndDeserialize<client::SendChunkEvent>());
        }
        else if (client::IsDelta(packetId))
        {
            m_incomingDeltaEventRegistry.HandleIncomingPacket(packetId, m_world,
                                                              ctx.data);
        }
        else  // if (client::IsRPC(packetId))
        {
            switch (packetId) {}
        }
    }

    void onClientConnected(oge::runtime::OnClientConnected ctx) {}

    void onClientConnectionTimeout(oge::runtime::OnClientConnectionTimeout ctx)
    {
    }

    void onClientDisconnected(oge::runtime::OnClientDisconnected ctx) {}

   public:
    ClientNetTransportLayer(entt::registry& world,
                            entt::dispatcher& clientDispatcher,
                            entt::dispatcher& worldDispatcher)
        : m_world(world),
          m_clientDispatcher(clientDispatcher),
          m_worldEventDispatcher(worldDispatcher)
    {
        m_clientDispatcher.sink<oge::runtime::OnClientReceivePacket>()
            .connect<&ClientNetTransportLayer::onClientReceivePacket>(this);
        m_clientDispatcher.sink<oge::runtime::OnClientConnected>()
            .connect<&ClientNetTransportLayer::onClientConnected>(this);
        m_clientDispatcher.sink<oge::runtime::OnClientConnectionTimeout>()
            .connect<&ClientNetTransportLayer::onClientConnectionTimeout>(this);
        m_clientDispatcher.sink<oge::runtime::OnClientDisconnected>()
            .connect<&ClientNetTransportLayer::onClientDisconnected>(this);
    }

    ~ClientNetTransportLayer() { m_clientDispatcher.disconnect(this); }

    template <typename TComponent>
    void RegisterNetComponent()
    {
        m_incomingDeltaEventRegistry.RegisterNetComponent<TComponent>();
    }

    void SetPlayerInput(input::PlayerInputStream* in) { m_playerInput = in; }

    void PostUpdate()
    {
        if (m_playerInput)
        {
        }
    }
};

class OutgoingDeltaEventRegistry
{
    struct Entry
    {
        void (*packetBuilder)(entt::entity, entt::registry&, net::Buffer&);
        size_t size;
    };

    std::vector<Entry> m_componentHandlersOutgoing;
    uint8_t nextComponentId = 0;

   public:
    template <typename TComponent>
    void RegisterNetComponent()
    {
        m_componentHandlersOutgoing.push_back(
            {client::DeltaPacketHandler<TComponent>::Serialize,
             client::DeltaPacketHandler<TComponent>::GetSize()},
            nextComponentId++);
    }

    void ProducePackets(oge::runtime::NetServer& server, ENetPeer* peer,
                        entt::registry& world)
    {
        for (auto [entity, dirty] : world.view<const DirtyTag>()->each())
        {
            for (uint8_t id : dirty.dirtyComponents)
            {
                auto handler = m_componentHandlersOutgoing[id];
                auto packet = server.StartPacket(handler.size);
                handler.packetBuilder(entity, world, packet);
                server.SendUnreliable(peer, packet);
            }
        }
    }
};

class ServerNetTransportationLayer
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
    ServerNetTransportationLayer(entt::registry& world,
                                 entt::dispatcher& serverDispatcher)
        : m_world(world), m_dispatcher(serverDispatcher)
    {
        m_dispatcher.sink<server::OnServerReceiveConnect>()
            .connect<&ServerNetTransportationLayer::onServerReciveConnect>(
                this);
        m_dispatcher.sink<server::OnServerReceiveDisconnect>()
            .connect<&ServerNetTransportationLayer::onServerReciveDisconnect>(
                this);
        m_dispatcher.sink<server::OnServerReceivePacket>()
            .connect<&ServerNetTransportationLayer::onServerRecivePacket>(this);
    }

    ~ServerNetTransportationLayer() { m_dispatcher.disconnect(this); }

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
