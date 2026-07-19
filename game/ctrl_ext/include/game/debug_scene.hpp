#include "game/app_context.hpp"
#include "game/input/input_source.hpp"
#include "game/scene.hpp"
#include "game/sim/subsystem.hpp"
#include "game/sim/terrain/subsystem_terrain.hpp"
#include "game/view/gfx/terrain_pass.hpp"
#include "game/view/renderer.hpp"
#include "game/view/terrain/terrain_renderer.hpp"
#include "game/ui/objects.hpp"
#include "game/components.hpp"
#include "game/view/submission_queue.hpp"
#include "oge/runtime/asset_ctx.hpp"

namespace game
{
class DebugScene3 : public Scene
{
   public:
    DebugScene3(AppContext ctx) : Scene(std::move(ctx))
    {
        m_inputs.AddStage<input::UIDragInput>();
        m_subsystems.AddStage<sim::SubsystemDebugText>();
        m_subsystems.AddStage<sim::SubsystemTerrain>();
        m_renderers.AddStage<view::TerrainRenderer>();
        m_renderers.AddStage<view::DebugInfoRenderer>();
        m_renderers.AddStage<view::UIRenderer>();
        m_renderers.AddStage<view::CameraRenderer>();

        auto& blocks = m_world.ctx().emplace<terrain::BlockRegistry>();
        blocks.RegisterBlock("dirt", {"Dirt", "dirt.png", 1});
        blocks.RegisterBlock("wood", {"Wood", "wood_plank.png", 1});
        blocks.RegisterBlock("stone", {"Stone", "green_stone.png", 1});

        m_world.ctx().emplace<terrain::TerrainView>();

        auto desc = m_world.ctx().emplace<terrain::TerrainDesc>();
        desc.chunkViewDistance = 1;
    }

    void Attach(OGEContext& ctx) override
    {
        Scene::Attach(ctx);
        auto assets = AssetContext(ctx);
        auto& blks = m_world.ctx().get<terrain::BlockRegistry>().GetBlockTextureArray();
        for (size_t i = 0; i < blks.size(); ++i)
        {
            GetPasses().GetPass<TerrainPass2>().UpdateBlockTexture(assets, blks[i], i);
        }

        auto vpe = ui::CreateGameView(m_world, {math::vec2{0, 0}, math::vec2{1, 1}});
        auto player = ComponentPlayer::CreatePlayer(m_world, {20.f, 20.f, 20.f});
        {
            ComponentCamera& cam = m_world.get<ComponentCamera>(player);
            cam.position = {20.f, 20.f, 20.f};

            glm::vec3 target = {0.f, 0.f, 0.f};
            cam.forward = glm::normalize(target - cam.position);

            cam.yaw = std::atan2(cam.forward.x, cam.forward.z);
            cam.pitch = std::asin(cam.forward.y);

            ComponentPerspectiveCamera& pcam = m_world.get<ComponentPerspectiveCamera>(player);
            pcam.fov = math::radians(45.f);
        }
        m_world.get<view::ViewPanel>(vpe).activeCamera = player;
        m_world.patch<view::ViewPanel>(vpe);

        // m_terminalButton = ui::CreateButton(world, context, {math::vec2{0.f, 0.f}, math::vec2{0.1f, 0.1f}});

        auto font = assets.LoadASCIIBitmapFont16x6("om_large_plain_idx.png");

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

        // put something in the middle of the screen
        ui::UISprite crossSprite{.sprite = assets.LoadTexture("cross.png")};
        auto cubeEntity = m_world.create();
        m_world.emplace<ui::UIRect>(cubeEntity, math::vec2{0.5f - 0.01f, 0.5f - 0.01f * assets.backend.SwapchainAspect()},
                              math::vec2{0.01f, 0.01f * assets.backend.SwapchainAspect()});
        m_world.emplace<ui::UISprite>(cubeEntity, crossSprite);
        m_world.emplace<ui::UIZLevel>(cubeEntity, 1);

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

        m_world.emplace<InputSourceWidget>(player, mvWidget, lookWidget);
    }
};
}  // namespace game
