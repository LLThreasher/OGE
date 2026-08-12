#pragma once

#include <array>
#include <cassert>
#include <string>

#include "game/components.hpp"
#include "game/events.hpp"
#include "game/game_world.hpp"
#include "game/input/input_source.hpp"
#include "game/input/net.hpp"
#include "game/input/player_input_stream.hpp"
#include "game/scene.hpp"
#include "game/scene_view.hpp"
#include "game/scene_runner.hpp"
#include "game/sim/subsystem.hpp"
#include "game/ui/objects.hpp"
#include "game/view/gfx/terrain_pass2.hpp"
#include "game/view/renderer.hpp"
#include "game/view/terrain/terrain_renderer.hpp"
#include "oge/input/keyboard.hpp"
#include "oge/input/raw_input_stream.hpp"
#include "oge/log.hpp"
#include "oge/math.hpp"
#include "oge/runtime/asset_ctx.hpp"
#include "oge/runtime/type_name.hpp"

namespace game
{
template <typename InnerScene>
class DebugVoxelView : public SceneView
{
   private:
    GameWorld& m_gameWorld;
    PlayerInfo m_playerInfo;
    entt::connection m_waitPlayer{};

    entt::entity m_cross = entt::null;
    entt::entity m_player = entt::null;
    entt::entity m_lookWidget = entt::null;
    entt::entity m_moveWidget = entt::null;
    bool usingKeyMouse = false;

    void ClearInputs()
    {
        m_inputs.Clear();

        if (m_uiWorld.valid(m_lookWidget)) m_uiWorld.destroy(m_lookWidget);
        if (m_uiWorld.valid(m_moveWidget)) m_uiWorld.destroy(m_moveWidget);
        if (m_uiWorld.valid(m_cross)) m_uiWorld.destroy(m_cross);

        m_lookWidget = entt::null;
        m_moveWidget = entt::null;
        m_cross = entt::null;
    }

    void AddKeyMouseInput()
    {
        auto extent = m_ctx.assets.backend.SwapchainExtent();

        auto& pcam =
            m_gameWorld.get<const ComponentPerspectiveCamera>(m_player);
        auto widgetInputDef = input::KeyMouseInput::Def{
            .target = m_gameWorld.get<input::PlayerInputStream>(m_player)};
        widgetInputDef.vfov = -pcam.fov;
        widgetInputDef.hfov =
            2.f * math::atan(math::tan(pcam.fov / 2.f) * pcam.aspect);

        m_inputs.AddStage<input::KeyMouseInput>(
            m_ctx.any_factory,
            input::KeyMouseInput::Def{
                .target = m_gameWorld.get<input::PlayerInputStream>(m_player),
                .mouseIdx = 0,
                .hfov = widgetInputDef.hfov / (float)extent.x,
                .vfov = widgetInputDef.vfov / (float)extent.y});

        // put something in the middle of the screen
        ui::UISprite crossSprite{
            .sprite = m_ctx.assets.LoadTexture("cursors/dot_large.png")};
        m_cross = m_uiWorld.create();
        m_uiWorld.emplace<ui::UIRect>(
            m_cross,
            math::vec2{0.5f - 0.01f,
                       0.5f - 0.01f * m_ctx.assets.backend.SwapchainAspect()},
            math::vec2{0.02f, 0.02f * m_ctx.assets.backend.SwapchainAspect()});
        m_uiWorld.emplace<ui::UISprite>(m_cross, crossSprite);
        m_uiWorld.emplace<ui::UIZLevel>(m_cross, 1);
    }

    void AddWidgetInput()
    {
        LOG_DEBUG("create widget input");
        // create move widget
        auto scaledX = 0.3f;
        auto scaledY = scaledX * m_ctx.assets.backend.SwapchainAspect();

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

        auto& pcam =
            m_gameWorld.get<const ComponentPerspectiveCamera>(m_player);
        auto m_widgetInputDef = input::WidgetInput::Def{
            .target = m_gameWorld.get<input::PlayerInputStream>(m_player),
            .pcam = pcam,
        };
        m_widgetInputDef.vfov = -pcam.fov;
        m_widgetInputDef.hfov =
            2.f * math::atan(math::tan(pcam.fov / 2.f) * pcam.aspect);
        m_widgetInputDef.vfov *= 2.f;
        m_widgetInputDef.hfov *= 3.f;
        m_widgetInputDef.viewWidget = lookWidget;
        m_widgetInputDef.moveWidget = mvWidget;

        m_inputs.AddStage<input::UIDragInput>(m_ctx.any_factory);
        m_inputs.AddStage<input::WidgetInput>(m_ctx.any_factory,
                                              m_widgetInputDef);

        m_lookWidget = lookWidget;
        m_moveWidget = mvWidget;
    }

    void onConstructPlayer(oge::runtime::OgeRegistryRef world, entt::entity e)
    {
        LOG_INFO(
            "entity {}: check player id {} against {}", (uint64_t)e,
            uuids::to_string(uuids::uuid(world.get<ComponentPlayer>(e).id)),
            uuids::to_string(uuids::uuid(m_playerInfo.uuid)));
        if (world.get<ComponentPlayer>(e).id == m_playerInfo.uuid)
        {
            ui::CreateGameView(m_uiWorld, {math::vec2{0, 0}, math::vec2{1, 1}},
                               e);

            if (!world.all_of<input::PlayerInputStream>(e))
                world.emplace<input::PlayerInputStream>(e);
            if (!world.all_of<UpdateTag<UpdateType::Realtime>>(e))
                world.emplace<UpdateTag<UpdateType::Realtime>>(e);
            // Local player: use prediction with rollback
            world.emplace_or_replace<RenderStrategyTag<RenderStrategy::LocalPrediction>>(e);
            if (!world.all_of<ComponentCamera>(e))
            {
                auto& cam = world.emplace<ComponentCamera>(e);
                cam.position = {20.f, 20.f, 20.f};
            }
            if (!world.all_of<ComponentPerspectiveCamera>(e))
            {
                auto& pcam = world.emplace<ComponentPerspectiveCamera>(e);
                pcam.aspect = m_ctx.assets.backend.SwapchainAspect();
            }
            assert(world.all_of<ComponentCamera>(e));
            assert(world.all_of<const ComponentPerspectiveCamera>(e));
            assert(world.all_of<input::PlayerInputStream>(e));
            assert(world.all_of<const ComponentAABBCollider>(e));
            assert(world.all_of<ComponentPlayer>(e));
            assert(world.all_of<ComponentPhysicBody>(e));
            assert(world.all_of<ComponentCreature>(e));
            m_player = e;

            AddWidgetInput();
            m_waitPlayer.release();
        }
        else
        {
            // Remote player: interpolate between server updates
            world.emplace_or_replace<RenderStrategyTag<RenderStrategy::Interpolation>>(e);
        }
    }

    void onResize(SurfaceRecreateEvent e)
    {
        ClearInputs();
        AddWidgetInput();
        usingKeyMouse = false;
    }

   public:
    DebugVoxelView(const Scene::Def& def)
        : SceneView(def, oge::runtime::TypeName<InnerScene>::Get()),
          m_gameWorld(m_innerScene.GetWorld())
    {
        m_playerInfo = LoadOrCreatePlayer();
        auto& sceneConfig = m_innerScene.GetConfig();
        if (sceneConfig.empty())
        {
            sceneConfig = GetDefaultSceneConfig(AF());
            sceneConfig.subsystems.insert(sceneConfig.subsystems.begin(),
                                          Id<sim::SubsystemDebugText>());
        }

        m_innerScene.Load();

        m_renderers.AddStage<view::DebugInfoRenderer>(AF());
        m_renderers.AddStage<view::TerrainRenderer>(AF());
        m_renderers.AddStage<view::CameraRenderer>(AF());
        m_renderers.AddStage<view::BlockHighlightRenderer>(AF());
        m_renderers.AddStage<view::GizmoRenderer>(AF());

        auto assets = AssetContext(m_ctx.any_ctx);
        auto& blks = m_gameWorld.ctx()
                         .template get<::game::terrain::BlockRegistry>()
                         .GetBlockTextures();
        for (size_t i = 0; i < blks.size(); ++i)
        {
            GetPasses().template GetPass<TerrainPass2>().UpdateBlockTexture(
                assets, blks[i], i);
        }

        {
            auto it = def.args.find("wait_player");
            if (it != def.args.end() && std::get<bool>(it->second))
            {
                m_waitPlayer =
                    m_gameWorld.on_construct<ComponentPlayer>()
                        .template connect<&DebugVoxelView::onConstructPlayer>(
                            this);
            }
            else
            {
                m_player =
                    ComponentPlayer::CreatePlayer(m_gameWorld, m_playerInfo);
                {
                    ComponentCamera& cam =
                        m_gameWorld.get<ComponentCamera>(m_player);
                    cam.position = {20.f, 20.f, 20.f};
                }
                onConstructPlayer(m_gameWorld.Raw(), m_player);
            }
        }

        m_ctx.events.sink<SurfaceRecreateEvent>()
            .connect<&DebugVoxelView::onResize>(this);

        // m_terminalButton = ui::CreateButton(world, context,
        // {math::vec2{0.f, 0.f}, math::vec2{0.1f, 0.1f}});

        // auto font =
        // assets.LoadASCIIBitmapFont16x6("om_large_plain_idx.png");

        // {
        //     auto e = world.create();
        //     world.emplace<UIRect>(e, math::vec2{0.3f - 0.01f, 0.3f -
        //     0.01f * context.backend.SwapchainAspect()}, math::vec2{0.1f,
        //     0.1f * context.backend.SwapchainAspect()});
        //     world.emplace<UISprite>(e, crossSprite);
        //     world.emplace<UIZLevel>(e, 1);
        // }

        // {
        //     auto e = world.create();
        //     world.emplace<UIRect>(e, math::vec2{0.3f - 0.01f, 0.7f -
        //     0.01f * context.backend.SwapchainAspect()}, math::vec2{0.1f,
        //     0.1f * context.backend.SwapchainAspect()});
        //     world.emplace<UISprite>(e, crossSprite);
        //     // world.emplace<UIText>(e, UIText{.font=font, .text="Hello
        //     world\nTerminal"});
        //     // world.emplace<UIZLevel>(e, 1);
        // }
    }

    void Update(Frame f, SceneContext sctx) override
    {
        using oge::input::KeyCode;
        if (!m_waitPlayer)
        {
            auto& keys = f.is.ActiveKeys();
            if (keys.contains(KeyCode::KY_G) && !usingKeyMouse)
            {
                usingKeyMouse = true;
                m_nextShowingCursor = false;
                ClearInputs();
                AddKeyMouseInput();
            }
            else if (keys.contains(KeyCode::KY_ESCAPE) && usingKeyMouse)
            {
                usingKeyMouse = false;
                m_nextShowingCursor = true;
                ClearInputs();
                AddWidgetInput();
            }
        }

        SceneView::Update(std::move(f), sctx);
    }
};
}  // namespace game

template <typename S>
struct oge::runtime::TypeName<game::DebugVoxelView<S>>
{
    static constexpr std::string Get()
    {
        return "core::DebugVoxelView<" + ::oge::runtime::TypeName<S>::Get() + ">";
    }
};
