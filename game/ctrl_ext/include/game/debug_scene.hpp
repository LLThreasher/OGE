#include <algorithm>
#include <memory>

#include "entt/entity/fwd.hpp"
#include "game/app_context.hpp"
#include "game/components.hpp"
#include "game/input/input_source.hpp"
#include "game/input/player_input_stream.hpp"
#include "game/graphical_scene.hpp"
#include "game/json.hpp"
#include "game/sim/subsystem.hpp"
#include "game/sim/terrain/subsystem_terrain.hpp"
#include "game/ui/objects.hpp"
#include "game/view/gfx/terrain_pass.hpp"
#include "game/view/renderer.hpp"
#include "game/view/submission_queue.hpp"
#include "game/view/terrain/terrain_renderer.hpp"
#include "oge/input/keyboard.hpp"
#include "oge/runtime/asset_ctx.hpp"
#include "oge/runtime/typed_registry.hpp"
#include "game/sim/subsystem_physics.hpp"

namespace game
{
class DebugScene3 : public GraphicalScene
{
    using InputDef = input::InputSource::Def;

    entt::entity m_cross;
    entt::entity m_player;
    bool usingKeyMouse = false;

    void AddWidgetInput(oge::runtime::AssetContext assets)
    {
        // create move widget
        auto scaledX = 0.3f;
        auto scaledY = scaledX * assets.backend.SwapchainAspect();

        auto lookWidget = m_world.create();
        m_world.emplace<ui::UIRect>(lookWidget, math::vec2{0.0f, 0.0f}, math::vec2{1.0f, 1.0f});
        m_world.emplace<ui::UIZLevel>(lookWidget, 0);
        m_world.emplace<ui::UIRaycastTarget>(lookWidget);

        auto mvWidget = m_world.create();
        m_world.emplace<ui::UIRect>(mvWidget, math::vec2{0.0f, 1.f - scaledY}, math::vec2{scaledX, scaledY});
        m_world.emplace<ui::UIZLevel>(mvWidget, 1);
        m_world.emplace<ui::UIRaycastTarget>(mvWidget);

        m_world.emplace<InputSourceWidget>(m_player, mvWidget, lookWidget);

        auto& pcam = m_world.get<const ComponentPerspectiveCamera>(m_player);
        auto m_widgetInputDef = InputDef{.target = m_world.try_get<input::PlayerInputStream>(m_player)};
        m_widgetInputDef.vfov = -pcam.fov;
        m_widgetInputDef.hfov = 2.f * math::atan(math::tan(pcam.fov / 2.f) * pcam.aspect);
        m_widgetInputDef.vfov *= 2.f;
        m_widgetInputDef.hfov *= 3.f;
        m_widgetInputDef.widgetInput = {lookWidget, mvWidget};

        m_inputs.AddStage<input::UIDragInput>(InputDef{});
        m_inputs.AddStage<input::WidgetInput>(m_widgetInputDef);
    }

   public:
    DebugScene3(AppContext ctx) : GraphicalScene(std::move(ctx))
    {
        m_subsystems.AddStage<sim::SubsystemDebugText>();
        m_subsystems.AddStage<sim::SubsystemTerrain>();
        m_subsystems.AddStage<sim::SubsystemPlayer<UpdateType::FixedStep>>();
        m_subsystems.AddStage<sim::SubsystemCreature<UpdateType::FixedStep>>();
        m_subsystems.AddStage<sim::SubsystemPhysics<UpdateType::FixedStep>>();

        m_realtimeSubsystems.AddStage<sim::SubsystemPlayer<UpdateType::Realtime>>();
        m_realtimeSubsystems.AddStage<sim::SubsystemCreature<UpdateType::Realtime>>();
        m_realtimeSubsystems.AddStage<sim::SubsystemPhysics<UpdateType::Realtime>>();
    }

    void Attach(const json::Value& args, OGEContext& ctx, AnythingFactory& af) override
    {
        auto& blocks = m_world.ctx().emplace<terrain::BlockRegistry>();
        blocks.RegisterBlock("dirt", {"Dirt", "dirt.png", 1, });
        blocks.RegisterBlock("wood", {"Wood", "wood_plank.png", 1});
        blocks.RegisterBlock("stone", {"Stone", "green_stone.png", 1});

        m_world.ctx().emplace<terrain::TerrainView>();

        auto desc = m_world.ctx().emplace<terrain::TerrainDesc>();
        desc.chunkViewDistance = 1;
        
        GraphicalScene::Attach(args, ctx, af);

        m_renderers->AddStage<view::TerrainRenderer>();
        m_renderers->AddStage<view::DebugInfoRenderer>();
        m_renderers->AddStage<view::UIRenderer>();
        m_renderers->AddStage<view::CameraRenderer>();

        auto assets = AssetContext(ctx);
        auto& blks = m_world.ctx().get<terrain::BlockRegistry>().GetBlockTextureArray();
        for (size_t i = 0; i < blks.size(); ++i)
        {
            GetPasses().GetPass<TerrainPass2>().UpdateBlockTexture(assets, blks[i], i);
        }

        auto vpe = ui::CreateGameView(m_world, {math::vec2{0, 0}, math::vec2{1, 1}});
        m_player = ComponentPlayer::CreatePlayer(m_world, {20.f, 20.f, 20.f});
        {
            ComponentCamera& cam = m_world.get<ComponentCamera>(m_player);
            cam.position = {20.f, 20.f, 20.f};

            glm::vec3 target = {0.f, 0.f, 0.f};
            cam.forward = glm::normalize(target - cam.position);

            cam.yaw = std::atan2(cam.forward.x, cam.forward.z);
            cam.pitch = std::asin(cam.forward.y);

            ComponentPerspectiveCamera& pcam = m_world.get<ComponentPerspectiveCamera>(m_player);
            pcam.fov = math::radians(45.f);
        }
        m_world.get<view::ViewPanel>(vpe).activeCamera = m_player;
        m_world.patch<view::ViewPanel>(vpe);

        // m_terminalButton = ui::CreateButton(world, context, {math::vec2{0.f, 0.f}, math::vec2{0.1f, 0.1f}});

        // auto font = assets.LoadASCIIBitmapFont16x6("om_large_plain_idx.png");

        // {
        //     auto e = world.create();
        //     world.emplace<UIRect>(e, math::vec2{0.3f - 0.01f, 0.3f - 0.01f * context.backend.SwapchainAspect()},
        //     math::vec2{0.1f, 0.1f * context.backend.SwapchainAspect()}); world.emplace<UISprite>(e, crossSprite);
        //     world.emplace<UIZLevel>(e, 1);
        // }

        // {
        //     auto e = world.create();
        //     world.emplace<UIRect>(e, math::vec2{0.3f - 0.01f, 0.7f - 0.01f * context.backend.SwapchainAspect()},
        //     math::vec2{0.1f, 0.1f * context.backend.SwapchainAspect()}); world.emplace<UISprite>(e, crossSprite);
        //     // world.emplace<UIText>(e, UIText{.font=font, .text="Hello world\nTerminal"});
        //     // world.emplace<UIZLevel>(e, 1);
        // }

        AddWidgetInput(assets);
    }

    void Update(Frame f) override
    {
        using oge::input::KeyCode;

        auto& keys = f.is.ActiveKeys();
        if (keys.contains(KeyCode::KY_G) && !usingKeyMouse)
        {
            usingKeyMouse = true;
            auto extent = m_ctx.value().assets.backend.SwapchainExtent();

            auto& pcam = m_world.get<const ComponentPerspectiveCamera>(m_player);
            auto m_widgetInputDef = InputDef{.target = m_world.try_get<input::PlayerInputStream>(m_player)};
            m_widgetInputDef.vfov = -pcam.fov;
            m_widgetInputDef.hfov = 2.f * math::atan(math::tan(pcam.fov / 2.f) * pcam.aspect);

            m_inputs.Clear();
            auto widgetIn = m_world.get<InputSourceWidget>(m_player);
            m_world.destroy(widgetIn.moveWidget);
            m_world.destroy(widgetIn.viewWidget);
            m_world.remove<InputSourceWidget>(m_player);

            m_inputs.AddStage<input::KeyMouseInput>(
                InputDef{.target = m_world.try_get<input::PlayerInputStream>(m_player),
                         .hfov = m_widgetInputDef.hfov / (float)extent.x,
                         .vfov = m_widgetInputDef.vfov / (float)extent.y,
                         .mouseIuput = {0}});

            // put something in the middle of the screen
            ui::UISprite crossSprite{.sprite = m_ctx->assets.LoadTexture("cross.png")};
            m_cross = m_world.create();
            m_world.emplace<ui::UIRect>(m_cross,
                                        math::vec2{0.5f - 0.01f, 0.5f - 0.01f * m_ctx->assets.backend.SwapchainAspect()},
                                        math::vec2{0.01f, 0.01f * m_ctx->assets.backend.SwapchainAspect()});
            m_world.emplace<ui::UISprite>(m_cross, crossSprite);
            m_world.emplace<ui::UIZLevel>(m_cross, 1);
        }
        else if (keys.contains(KeyCode::KY_ESCAPE) && usingKeyMouse)
        {
            usingKeyMouse = false;
            m_inputs.Clear();
            m_world.destroy(m_cross);
            AddWidgetInput(m_ctx->assets);
        }
        GraphicalScene::Update(std::move(f));
    }
};
}  // namespace game
