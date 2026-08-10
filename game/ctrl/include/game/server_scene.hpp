#pragma once

#include <vector>

#include "game/app_context.hpp"
#include "game/components.hpp"
#include "game/net/event_log_stream.hpp"
#include "game/net/replication_registry.hpp"
#include "game/scene.hpp"
#include "game/sim/subsystem.hpp"
#include "game/sim/subsystem_physics.hpp"
#include "game/sim/terrain/subsystem_terrain.hpp"
#include "game/terrain/block_registry.hpp"
#include "game/terrain/terrain_view.hpp"
#include "oge/log.hpp"
#include "oge/runtime/net_packet_sender.hpp"
#include "oge/runtime/net_server.hpp"
#include "uuid.h"

namespace game
{
using oge::runtime::NetServer;
using oge::runtime::OnServerReceiveConnect;
using oge::runtime::OnServerReceiveDisconnect;
using oge::runtime::OnServerReceivePacket;

class DebugServerScene final : public Scene
{
    float p_pendingConnTimeoutSec = 10.f;

    entt::dispatcher m_serverEventDispatcher;
    NetServer& m_netServer;
    ENetPeer* m_pendingConnection = nullptr;
    ENetPeer* m_pendingConnection2 = nullptr;
    float m_pendingConnTimeout = 0.f;
    float m_pendingConn2Timeout = 0.f;
    std::deque<PlayerInfo> m_playerEntries;
    net::EventLogStream<>& m_eventLogStream;
    net::ReplicationRegistry m_replicationRegistry;

    void onServerRecieveConnect(OnServerReceiveConnect c)
    {
        LOG_INFO("connecting {}", c.peerId);
        if (m_pendingConnection)
        {
            m_netServer.Disconnect(c.peer);
            LOG_INFO("drop peer connection due to flow ctrl: {}",
                     (void*)c.peer);
            return;
        }
        m_pendingConnection = c.peer;
        m_pendingConnTimeout = p_pendingConnTimeoutSec;
    }

    void onServerReceiveDisconnect(OnServerReceiveDisconnect c)
    {
        LOG_INFO("disconnecting {}", c.peerId);
        if (m_playerEntries.size() > c.peerId)
        {
            auto uuid = uuids::uuid{m_playerEntries[c.peerId].uuid};
            LOG_INFO("found uuid {}", uuids::to_string(uuid));
            if (!uuid.is_nil())
            {
                LOG_INFO("destroy player enitty: {}", uuids::to_string(uuid));
                ComponentPlayer::DestroyPlayer(m_world,
                                               m_playerEntries[c.peerId]);
                m_playerEntries[c.peerId] = {};
            }
        }
        m_replicationRegistry.RemovePeer(c.peerId);
        if (c.peer == m_pendingConnection) {
            m_pendingConnection = nullptr;
            m_pendingConnTimeout = 0.f;
        }
        if (c.peer == m_pendingConnection2) {
            m_pendingConnection2 = nullptr;
            m_pendingConn2Timeout = 0.f;
        }
    }

    void onServerReceivePacket(OnServerReceivePacket p)
    {
        if (p.peer == m_pendingConnection)
        {
            m_pendingConnection = nullptr;
            m_pendingConnTimeout = 0.f;
            if (m_pendingConnection2 == nullptr)
            {
                m_pendingConnection2 = p.peer;
                m_pendingConn2Timeout = p_pendingConnTimeoutSec;
            }
            else
            {
                m_netServer.Disconnect(p.peer);
                LOG_INFO("drop peer connection due to flow ctrl 2: {}",
                         (void*)p.peer);
                return;
            }
            auto playerInfo = p.data->Read<PlayerInfo>();
            for (auto& entry : m_playerEntries)
            {
                if (entry.uuid == playerInfo.uuid)
                {
                    LOG_WARN(
                        "found existing player with uuid {}, disconnecting",
                        uuids::to_string(uuids::uuid{entry.uuid}));
                    m_netServer.Disconnect(p.peer);
                    m_pendingConnection2 = nullptr;
                    m_pendingConn2Timeout = 0.f;
                    return;
                }
            }
            auto playerEntity =
                ComponentPlayer::CreatePlayer(m_world, playerInfo);
            m_world.emplace<ReplicatedTag>(playerEntity);
            m_playerEntries.resize(
                math::max(p.peerId + 1, (uint32_t)m_playerEntries.size()));
            m_playerEntries[p.peerId] = playerInfo;
            LOG_INFO(
                "create player enitty: {}({}) for conn({})",
                uuids::to_string(uuids::uuid{m_playerEntries[p.peerId].uuid}),
                (uint32_t)playerEntity, p.peerId);
            auto packet = m_netServer.StartPacket(sizeof(entt::entity));
            packet.Write(playerEntity);
            m_netServer.Send(p.peer, packet);
        }
        else if (p.peer == m_pendingConnection2)
        {
            m_pendingConnection2 = nullptr;
            m_pendingConn2Timeout = 0.f;
            m_replicationRegistry.AddPeer(p.peerId, p.peer, &m_world);
            LOG_INFO("initialize replication peer for conn({})", p.peerId);
        }
        else
        {
            m_replicationRegistry.HandleIncoming(p.peerId, m_world, *p.data);
        }
    }

   public:
    DebugServerScene(const Def& def)
        : Scene(def),
          m_netServer(m_ctx.any_ctx.Emplace<NetServer>()),
          m_eventLogStream(m_world.ctx().emplace<net::EventLogStream<>>(&m_ctx.any_factory)),
          m_replicationRegistry(net::ReplicationRegistry::Def{
              m_eventLogStream,
              m_ctx.any_factory})
    {
        m_serverEventDispatcher.sink<OnServerReceiveConnect>()
            .connect<&DebugServerScene::onServerRecieveConnect>(this);
        m_serverEventDispatcher.sink<OnServerReceiveDisconnect>()
            .connect<&DebugServerScene::onServerReceiveDisconnect>(this);
        m_serverEventDispatcher.sink<OnServerReceivePacket>()
            .connect<&DebugServerScene::onServerReceivePacket>(this);
        RegisterReplications(m_ctx.any_factory, m_replicationRegistry);

        m_sceneConfig.subsystems.push_back(Id<sim::SubsystemTerrain>());
        m_sceneConfig.subsystems.push_back(
            Id<sim::SubsystemPlayer<UpdateType::FixedStep>>());
        m_sceneConfig.subsystems.push_back(
            Id<sim::SubsystemPlayer<UpdateType::Realtime>>());
        m_sceneConfig.subsystems.push_back(
            Id<sim::SubsystemCreature<UpdateType::FixedStep>>());
        m_sceneConfig.subsystems.push_back(
            Id<sim::SubsystemCreature<UpdateType::Realtime>>());
        m_sceneConfig.subsystems.push_back(
            Id<sim::SubsystemPhysics<UpdateType::FixedStep>>());
        m_sceneConfig.subsystems.push_back(
            Id<sim::SubsystemPhysics<UpdateType::Realtime>>());

        Load();

        // uint16_t port = 23400;
        uint16_t port = 23401;
        size_t maxClients = 20;
        {
            auto it = def.args.find("port");
            if (it != def.args.end()) port = std::get<int64_t>(it->second);
        }
        {
            auto it = def.args.find("maxClients");
            if (it != def.args.end())
                maxClients = std::get<int64_t>(it->second);
        }
        m_netServer.Initialize(port, maxClients, 3);

        m_subsystems.SetUpdateInterval(1 / 20.f);
        net::InstallComponentReplicationHooks<ComponentAABBCollider>(m_world);
        net::InstallComponentReplicationHooks<ComponentPhysicBody>(m_world);
        net::InstallComponentReplicationHooks<ComponentCreature>(m_world);
        net::InstallComponentReplicationHooks<ComponentCamera>(m_world);
        net::InstallComponentReplicationHooks<ComponentPerspectiveCamera>(
            m_world);
        net::InstallComponentReplicationHooks<ComponentPlayer>(m_world);
        net::InstallEntityReplicationHooks(m_world);
        net::InstallTerrainReplicationHooks(m_world);
        net::InstallPlayerInputReplicationHooks(m_world);
    }

    ~DebugServerScene()
    {
        m_ctx.any_ctx.Erase<oge::runtime::NetServer>();
    }

    void Update(Frame f, SceneContext sctx) override
    {
        if (m_pendingConnection)
        {
            if (m_pendingConnTimeout <= 0)
            {
                m_netServer.Disconnect(m_pendingConnection);
                m_pendingConnection = nullptr;
                m_pendingConnTimeout = 0;
            }
            else
            {
                m_pendingConnTimeout -= f.dt;
            }
        }
        if (m_pendingConnection2)
        {
            if (m_pendingConn2Timeout <= 0)
            {
                m_netServer.Disconnect(m_pendingConnection2);
                m_pendingConnection2 = nullptr;
                m_pendingConn2Timeout = 0;
            }
            else
            {
                m_pendingConn2Timeout -= f.dt;
            }
        }

        m_netServer.Poll(m_serverEventDispatcher, f.dt);
        OGE_ASSERT(m_serverEventDispatcher.size() == 0,
                   "Event dispatcher not empty after poll");
        m_eventLogStream.Update();
        m_replicationRegistry.ProduceAll(m_netServer, m_world);
        Scene::Update(f, sctx);
        net::PollTerrainChunkEvents(m_world);
    }

    void Load() override
    {
        auto& blocks = m_world.ctx().emplace<::game::terrain::BlockRegistry>();
        blocks.RegisterBlock("dirt", {
                                         "Dirt",
                                         "dirt.png",
                                         1,
                                     });
        blocks.RegisterBlock("wood", {"Wood", "wood_plank.png", 1});
        blocks.RegisterBlock("stone", {"Stone", "green_stone.png", 1});

        m_world.ctx().emplace<::game::terrain::TerrainView>();

        auto& desc = m_world.ctx().emplace<::game::terrain::TerrainDesc>();
        desc.chunkViewDistance = 8;

        for (auto stage : m_sceneConfig.subsystems)
        {
            m_subsystems.AddStage(m_ctx.any_factory, stage);
        }
        for (auto stage : m_sceneConfig.realtimeSubsystems)
        {
            m_subsystems.AddStage(m_ctx.any_factory, stage);
        }
    }
};
}  // namespace game

DECL_TYPE_NAME(game::DebugServerScene, "core::DebugServerScene")
