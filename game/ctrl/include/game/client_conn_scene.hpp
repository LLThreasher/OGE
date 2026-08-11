#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include "game/components.hpp"
#include "game/game_world.hpp"
#include "game/input/player_input_stream.hpp"
#include "oge/json.hpp"
#include "game/net/replication_registry.hpp"
#include "game/scene.hpp"
#include "game/sim/subsystem.hpp"
#include "game/sim/subsystem_physics.hpp"
#include "oge/color.hpp"
#include "oge/log.hpp"
#include "oge/runtime/net_client.hpp"
#include "oge/runtime/net_packet_sender.hpp"
#include "oge/runtime/net_serializer.hpp"
#include "uuid.h"

namespace game
{
using oge::runtime::NetClient;
using oge::runtime::OnClientConnected;
using oge::runtime::OnClientConnectionTimeout;
using oge::runtime::OnClientDisconnected;
using oge::runtime::OnClientReceivePacket;

struct ClientArgs
{
    std::string ip = "127.0.0.1";
    uint16_t port = 25567;
    uint32_t timeout = 5000;
};

enum class ClientState
{
    WaitingConnect,
    WaitingConfig,
    ReceivedConfig,
    Available,
    Disconnected,
};

class ClientConnScene : public Scene
{
    enum class State
    {
        Connecting,
        Connected,
        Timeout,
        Ready,
    };

    oge_id_type m_nextSene;
    json::Object m_nextSceneArgs;
    PlayerInfo m_playerInfo;
    entt::dispatcher m_clientDispatcher;
    NetClient& m_client;
    State m_state;

    void onConnected(OnClientConnected ctx)
    {
        LOG_INFO("client connected");
        m_state = State::Connected;

        auto packet = m_client.StartPacket(sizeof(PlayerInfo));
        packet.Write(m_playerInfo);
        m_client.Send(packet, oge::runtime::SendType::Reliable);
    }

    void onConnectionTimeout(OnClientConnectionTimeout ctx)
    {
        LOG_ERROR("client connection timeout");
        m_state = State::Timeout;
    }

    void onDisconnected(OnClientDisconnected ctx)
    {
        LOG_ERROR("client disconnected");
        m_state = State::Timeout;
    }

    void onRecievePacket(OnClientReceivePacket ctx)
    {
        LOG_INFO("handshake success");
        m_state = State::Ready;
        m_nextSceneArgs["player_entity"] =
            static_cast<int64_t>(ctx.data->Read<entt::entity>());
    }

   public:
    ClientConnScene(const Def& def)
        : Scene(def), m_client(def.ctx.any_ctx.Emplace<NetClient>())
    {
        auto it = def.args.find("next_scene");
        if (it == def.args.end())
        {
            LOG_WARN("next_scene not provided, default to ClientScene2");
            m_nextSene = Id("core::DebugVoxelView<core::ClientScene2>");
        }
        else
        {
            m_nextSene = std::get<int64_t>(it->second);
        }
        m_playerInfo = LoadOrCreatePlayer();

        // Forward an explicit scene_config to the next scene so callers
        // (e.g. tests) can control what ClientScene2 loads.
        auto cfgIt = def.args.find("scene_config");
        if (cfgIt != def.args.end())
        {
            m_nextSceneArgs["scene_config"] = cfgIt->second;
        }

        // uint16_t port = 23400;
        uint16_t port = 23401;
        std::string_view ip = "localhost";
        {
            auto it = def.args.find("port");
            if (it != def.args.end()) port = std::get<int64_t>(it->second);
        }
        {
            auto it = def.args.find("ip");
            if (it != def.args.end()) ip = std::get<std::string>(it->second);
        }
        m_client.Initialize(3);
        m_client.Connect(std::string(ip).c_str(), port);
        m_clientDispatcher.sink<OnClientConnected>()
            .connect<&ClientConnScene::onConnected>(this);
        m_clientDispatcher.sink<OnClientConnectionTimeout>()
            .connect<&ClientConnScene::onConnectionTimeout>(this);
        m_clientDispatcher.sink<OnClientReceivePacket>()
            .connect<&ClientConnScene::onRecievePacket>(this);
        m_clientDispatcher.sink<OnClientDisconnected>()
            .connect<&ClientConnScene::onDisconnected>(this);
    }

    void Update(Frame f, SceneContext sctx) override
    {
        Scene::Update(f, sctx);
        m_client.Poll(m_clientDispatcher, f.dt);
        if (m_state == State::Ready)
        {
            sctx.nextScene = m_nextSene;
            for (auto& [key, val] : m_nextSceneArgs)
            {
                sctx.nextSceneArgs[key] = val;
            }
            // Only synthesize the default client config when the caller did
            // not provide one (tests pass their own scene_config).
            if (m_nextSceneArgs.find("scene_config") == m_nextSceneArgs.end())
            {
                SceneConfig cfg = GetDefaultSceneConfig(AF());
                cfg.subsystems.clear();
                cfg.realtimeSubsystems.clear();
                cfg.subsystems.push_back(Id<sim::SubsystemDebugText>());
                cfg.realtimeSubsystems.push_back(Id<sim::SubsystemPlayer<UpdateType::Realtime>>());
                cfg.realtimeSubsystems.push_back(Id<sim::SubsystemCreature<UpdateType::Realtime>>());
                cfg.realtimeSubsystems.push_back(Id<sim::SubsystemPhysics<UpdateType::Realtime>>());
                sctx.nextSceneArgs["scene_config"] = json::ToJson(cfg);
            }
            sctx.nextSceneArgs["wait_player"] = json::ToJson(true);
        }
        else if (m_state == State::Timeout)
        {
            LOG_ERROR("connection failure");
            sctx.nextScene = Id<Scene>();
            m_client.Shutdown();
            m_ctx.any_ctx.Erase<NetClient>();
        }
    }
};
}  // namespace game

DECL_TYPE_NAME(game::ClientConnScene, "core::ClientConnScene")
