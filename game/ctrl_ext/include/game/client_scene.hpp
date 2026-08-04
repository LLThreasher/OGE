#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include "game/components.hpp"
#include "game/game_world.hpp"
#include "game/input/input_source.hpp"
#include "game/input/player_input_stream.hpp"
#include "game/json.hpp"
#include "game/replication_registry.hpp"
#include "game/scene.hpp"
#include "game/scene_ext.hpp"
#include "game/sim/subsystem.hpp"
#include "game/ui/objects.hpp"
#include "game/view/renderer.hpp"
#include "game/view/submission_queue.hpp"
#include "game/view/terrain/terrain_renderer.hpp"
#include "oge/color.hpp"
#include "oge/log.hpp"
#include "oge/runtime/net_client.hpp"
#include "oge/runtime/net_packet_sender.hpp"
#include "oge/runtime/net_serializer.hpp"
#include "uuid.h"

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
    PlayerInfo m_playerInfo;
    entt::dispatcher m_clientDispatcher;
    ReplicationRegistry m_replicationRegistry;

    entt::entity m_cross;
    entt::entity m_player = entt::null;
    entt::entity m_lookWidget;
    entt::entity m_moveWidget;
    bool usingKeyMouse = false;

    bool m_readyToQuit = false;

    void AddWidgetInput(oge::runtime::AssetContext assets)
    {
        // create move widget
        auto scaledX = 0.3f;
        auto scaledY = scaledX * assets.backend.SwapchainAspect();

        auto lookWidget = m_uiWorld.create();
        m_uiWorld.emplace<ui::UIRect>(lookWidget, math::vec2{0.0f, 0.0f},
                                      math::vec2{1.0f, 1.0f});
        m_uiWorld.emplace<ui::UIZLevel>(lookWidget, 0);
        m_uiWorld.emplace<ui::UIRaycastTarget>(lookWidget);

        auto mvWidget = m_uiWorld.create();
        m_uiWorld.emplace<ui::UIRect>(mvWidget, math::vec2{0.0f, 1.f - scaledY},
                                      math::vec2{scaledX, scaledY});
        m_uiWorld.emplace<ui::UIZLevel>(mvWidget, 1);
        m_uiWorld.emplace<ui::UIRaycastTarget>(mvWidget);

        auto& pcam = m_world.get<const ComponentPerspectiveCamera>(m_player);
        auto m_widgetInputDef = input::WidgetInput::Def{
            .target = m_world.get<input::PlayerInputStream>(m_player),
            .pcam = pcam,
        };
        m_widgetInputDef.vfov = -pcam.fov;
        m_widgetInputDef.hfov =
            2.f * math::atan(math::tan(pcam.fov / 2.f) * pcam.aspect);
        m_widgetInputDef.vfov *= 2.f;
        m_widgetInputDef.hfov *= 3.f;
        m_widgetInputDef.viewWidget = lookWidget;
        m_widgetInputDef.moveWidget = mvWidget;

        m_inputs.AddStage<input::UIDragInput>(AF());
        m_inputs.AddStage<input::WidgetInput>(AF(), m_widgetInputDef);

        m_lookWidget = lookWidget;
        m_moveWidget = mvWidget;
    }

    void onDisconnected(OnClientDisconnected ctx)
    {
        LOG_ERROR("client disconnected");
        m_readyToQuit = true;
    }

    void onRecievePacket(OnClientReceivePacket ctx)
    {
        m_replicationRegistry.HandleIncoming(m_world, *ctx.data);
    }

    void onConstructPlayer(entt::registry& world, entt::entity e)
    {
        LOG_INFO(
            "check player id {} against {}",
            uuids::to_string(uuids::uuid(world.get<ComponentPlayer>(e).id)),
            uuids::to_string(uuids::uuid(m_playerInfo.uuid)));
        if (world.get<ComponentPlayer>(e).id == m_playerInfo.uuid)
        {
            world.on_construct<ComponentPlayer>().disconnect();
            ui::CreateGameView(m_uiWorld, {math::vec2{0, 0}, math::vec2{1, 1}},
                               e);
            world.emplace<input::PlayerInputStream>(e);
            world.emplace<ReplicatedTag>(e);
            m_player = e;

            AddWidgetInput(m_ctx.assets);
        }
    }

   public:
    ClientScene(const Def& def)
        : SceneExt(def), m_client(*m_ctx.any_ctx.Get<NetClient>())
    {
        m_sceneConfig.subsystems.push_back(Id<sim::SubsystemDebugText>());

        Load();

        m_renderers.AddStage<view::UIRenderer>(AF());
        m_renderers.AddStage<view::DebugInfoRenderer>(AF());
        m_renderers.AddStage<view::TerrainRenderer>(AF());
        m_renderers.AddStage<view::CameraRenderer>(AF());

        LOG_INFO("client scene loaded");
        m_playerInfo = LoadOrCreatePlayer();
        RegisterReplications(m_ctx.any_factory, m_replicationRegistry);
        m_clientDispatcher.sink<OnClientReceivePacket>()
            .connect<&ClientScene::onRecievePacket>(this);
        m_clientDispatcher.sink<OnClientDisconnected>()
            .connect<&ClientScene::onDisconnected>(this);
        m_world.on_construct<ComponentPlayer>()
            .connect<&ClientScene::onConstructPlayer>(this);
        m_replicationRegistry.AddPeer(m_client.Host());

        InstallEntityReplicationHooks(m_world);
        m_replicationRegistry.AddFamilyToSend(Id<input::PlayerInputStream>());

        auto packet = m_client.StartPacket(sizeof(uint32_t));
        packet.Write<uint32_t>(0);
        m_client.Send(packet, oge::runtime::SendType::Reliable);

        auto assets = m_ctx.assets;
        auto& blks =
            m_world.ctx().get<::game::terrain::BlockRegistry>().GetBlockTextures();
        for (size_t i = 0; i < blks.size(); ++i)
        {
            GetPasses().GetPass<TerrainPass2>().UpdateBlockTexture(assets,
                                                                   blks[i], i);
        }
    }

    ~ClientScene()
    {
        m_client.Disconnect();
        m_ctx.any_ctx.Erase<NetClient>();
    }

    void Update(Frame f, SceneContext sctx) override
    {
        m_client.Poll(m_clientDispatcher, f.dt);
        if (m_readyToQuit)
        {
            sctx.nextScene = Id<SceneExt>();
            sctx.nextSceneArgs = {};
            return;
        }
        m_replicationRegistry.ProduceAll(m_client, m_world);

        if (m_player != entt::null)
        {
            using oge::input::KeyCode;

            auto& keys = f.is.ActiveKeys();
            if (keys.contains(KeyCode::KY_G) && !usingKeyMouse)
            {
                usingKeyMouse = true;
                auto extent = m_ctx.assets.backend.SwapchainExtent();

                m_uiWorld.destroy(m_lookWidget);
                m_uiWorld.destroy(m_moveWidget);

                auto& pcam =
                    m_world.get<const ComponentPerspectiveCamera>(m_player);
                auto m_widgetInputDef = input::KeyMouseInput::Def{
                    .target = m_world.get<input::PlayerInputStream>(m_player)};
                m_widgetInputDef.vfov = -pcam.fov;
                m_widgetInputDef.hfov =
                    2.f * math::atan(math::tan(pcam.fov / 2.f) * pcam.aspect);

                m_inputs.Clear();

                m_inputs.AddStage<input::KeyMouseInput>(
                    AF(), input::KeyMouseInput::Def{
                              .target = m_world.get<input::PlayerInputStream>(
                                  m_player),
                              .mouseIdx = 0,
                              .hfov = m_widgetInputDef.hfov / (float)extent.x,
                              .vfov = m_widgetInputDef.vfov / (float)extent.y});

                // put something in the middle of the screen
                ui::UISprite crossSprite{
                    .sprite = m_ctx.assets.LoadTexture("cross.png")};
                m_cross = m_uiWorld.create();
                m_uiWorld.emplace<ui::UIRect>(
                    m_cross,
                    math::vec2{
                        0.5f - 0.01f,
                        0.5f - 0.01f * m_ctx.assets.backend.SwapchainAspect()},
                    math::vec2{0.01f,
                               0.01f * m_ctx.assets.backend.SwapchainAspect()});
                m_uiWorld.emplace<ui::UISprite>(m_cross, crossSprite);
                m_uiWorld.emplace<ui::UIZLevel>(m_cross, 1);
            }
            else if (keys.contains(KeyCode::KY_ESCAPE) && usingKeyMouse)
            {
                usingKeyMouse = false;
                m_inputs.Clear();
                m_uiWorld.destroy(m_cross);
                AddWidgetInput(m_ctx.assets);
            }
        }

        SceneExt::Update(f, sctx);
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

        auto desc = m_world.ctx().emplace<::game::terrain::TerrainDesc>();
        desc.chunkViewDistance = 1;

        SceneExt::Load();
    }
};

class ClientConnScene : public SceneExt
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
        m_client.Send(packet, SendType::Reliable);
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
        m_clientDispatcher.sink<OnClientConnected>()
            .connect<&ClientConnScene::onConnected>(this);
        m_clientDispatcher.sink<OnClientConnectionTimeout>()
            .connect<&ClientConnScene::onConnectionTimeout>(this);
        m_clientDispatcher.sink<OnClientReceivePacket>()
            .connect<&ClientConnScene::onRecievePacket>(this);
        m_clientDispatcher.sink<OnClientDisconnected>()
            .connect<&ClientConnScene::onDisconnected>(this);

        GetPasses().SetClearColor(oge::colors::GREY);
    }

    void Update(Frame f, SceneContext sctx) override
    {
        SceneExt::Update(f, sctx);
        m_client.Poll(m_clientDispatcher, f.dt);
        if (m_state == State::Ready)
        {
            sctx.nextScene = m_nextSene;
            for (auto& [key, val] : m_nextSceneArgs)
            {
                sctx.nextSceneArgs[key] = val;
            }
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
    static constexpr std::string Get()
    {
        return "core::ClientScene";
    }
};

template <>
struct TypeName<game::ClientConnScene>
{
    static constexpr std::string Get()
    {
        return "core::ClientConnScene";
    }
};
}  // namespace oge::runtime
