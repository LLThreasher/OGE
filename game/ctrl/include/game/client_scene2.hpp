#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include "game/json.hpp"
#include "game/net/replication_registry.hpp"
#include "game/scene.hpp"
#include "oge/log.hpp"
#include "oge/runtime/net_client.hpp"
#include "oge/runtime/net_packet_sender.hpp"
#include "oge/runtime/net_serializer.hpp"

namespace game
{
using oge::runtime::NetClient;
using oge::runtime::OnClientConnected;
using oge::runtime::OnClientConnectionTimeout;
using oge::runtime::OnClientDisconnected;
using oge::runtime::OnClientReceivePacket;

class ClientScene2 : public Scene
{
    NetClient& m_client;
    entt::dispatcher m_clientDispatcher;
    ::game::net::ReplicationRegistry m_replicationRegistry;

    bool m_readyToQuit = false;

    void onDisconnected(OnClientDisconnected ctx)
    {
        LOG_ERROR("client disconnected");
        m_readyToQuit = true;
    }

    void onRecievePacket(OnClientReceivePacket ctx)
    {
        m_replicationRegistry.HandleIncoming(0, m_world, *ctx.data);
    }

   public:
    ClientScene2(const Def& def)
        : Scene(def),
          m_client(*m_ctx.any_ctx.Get<NetClient>()),
          m_replicationRegistry(::game::net::ReplicationRegistry::Def{
              m_world.ctx().emplace<::game::net::EventLogStream<>>(&m_ctx.any_factory),
              m_ctx.any_factory})
    {
        LOG_INFO("client scene loaded");
        RegisterReplications(m_ctx.any_factory, m_replicationRegistry);
        m_clientDispatcher.sink<OnClientReceivePacket>()
            .connect<&ClientScene2::onRecievePacket>(this);
        m_clientDispatcher.sink<OnClientDisconnected>()
            .connect<&ClientScene2::onDisconnected>(this);
        m_replicationRegistry.AddPeer(0, m_client.Host(), &m_world);

        // The client world is a read-only mirror of the server: state
        // changes arrive as replication events applied by HandleIncoming.
        // No local hooks — they would fire when applied entities/components
        // materialize and echo them back to the server via ProduceAll.

        // send ready package
        auto packet = m_client.StartPacket(sizeof(uint32_t));
        packet.Write<uint32_t>(0);
        m_client.Send(packet, oge::runtime::SendType::Reliable);
    }

    ~ClientScene2()
    {
        m_client.Disconnect();
        m_ctx.any_ctx.Erase<NetClient>();
    }

    void Update(Frame f, SceneContext sctx) override
    {
        m_client.Poll(m_clientDispatcher, f.dt);
        if (m_readyToQuit)
        {
            sctx.nextScene = Id<Scene>();
            sctx.nextSceneArgs = {};
        }
        m_replicationRegistry.ProduceAll(m_client, m_world);

        Scene::Update(f, sctx);
    }
};
}  // namespace game

DECL_TYPE_NAME(game::ClientScene2, "core::ClientScene2")
