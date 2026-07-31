#pragma once

#include <string>
#include <string_view>

#include "entt/signal/fwd.hpp"
#include "game/components.hpp"
#include "game/game_world.hpp"
#include "game/json.hpp"
#include "game/scene.hpp"
#include "game/scene_ext.hpp"
#include "oge/log.hpp"
#include "oge/runtime/net_client.hpp"
#include "oge/runtime/net_serializer.hpp"

namespace game
{
namespace net = oge::runtime::net;
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

class ClientScene : public SceneExt
{
    NetClient& m_client;
   public:
    ClientScene(const Def& def) : SceneExt(def), m_client(*m_ctx.any_ctx.Get<NetClient>())
    {
        Load();
        LOG_INFO("client scene loaded");
    }
};

class ClientConnScene : public SceneExt
{
    enum class State
    {
        Connecting,
        Connected,
        Timeout,
    };
    
    oge_id_type m_nextSene;
    PlayerInfo m_playerInfo;
    entt::dispatcher m_clientDispatcher;
    NetClient& m_client;
    State m_state;

    void onConnected(OnClientConnected ctx)
    {
        LOG_INFO("client connected");
        m_state = State::Connected;
    }

    void onConnectionTimeout(OnClientConnectionTimeout ctx)
    {
        LOG_INFO("client connection timeout");
        m_state = State::Timeout;
    }

   public:
    ClientConnScene(const Def& def)
        : SceneExt(def), m_client(def.ctx.any_ctx.Emplace<NetClient>())
    {
        auto it = def.args.find("next_scene");
        if (it == def.args.end())
        {
            m_nextSene = Id<ClientScene>();
        }
        else
        {
            m_nextSene = std::get<int64_t>(it->second);
        }
        m_playerInfo = LoadOrCreatePlayer();

        uint16_t port = 23400;
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
        m_clientDispatcher.sink<OnClientConnected>().connect<&ClientConnScene::onConnected>(this);
        m_clientDispatcher.sink<OnClientConnectionTimeout>().connect<&ClientConnScene::onConnectionTimeout>(this);
    }

    void Update(Frame f, SceneContext sctx) override
    {
        SceneExt::Update(f, sctx);
        m_client.Poll(m_clientDispatcher, f.dt);
        if (m_state == State::Connected)
        {
            sctx.nextScene = m_nextSene;
        }
        else if (m_state == State::Timeout)
        {
            LOG_ERROR("connection failure");
            sctx.nextScene = Id<SceneExt>();
            m_client.Shutdown();
            m_ctx.any_ctx.Erase<NetClient>();
        }
    }
};
}  // namespace game

namespace oge::runtime
{
template <>
struct TypeName<game::ClientScene>
{
    static consteval std::string_view Get()
    {
        return "core::ClientScene";
    }
};

template <>
struct TypeName<game::ClientConnScene>
{
    static consteval std::string_view Get()
    {
        return "core::ClientConnScene";
    }
};
}  // namespace oge::runtime
