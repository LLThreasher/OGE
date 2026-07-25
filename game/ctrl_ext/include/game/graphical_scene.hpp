#pragma once

#include <array>
#include <memory>
#include <memory_resource>
#include <optional>
#include <valarray>
#include <vector>

#include "game/app_context.hpp"
#include "game/game_world.hpp"
#include "game/input/input_source.hpp"
#include "game/json.hpp"
#include "game/memory_context.hpp"
#include "game/sim/subsystem.hpp"
#include "game/terrain/defs.hpp"
#include "game/view/gfx/debug_info_pass.hpp"
#include "game/view/gfx/terrain_pass2.hpp"
#include "game/view/gfx/view_executor.hpp"
#include "game/view/renderer.hpp"
#include "game/view/submission_queue.hpp"
#include "oge/array_helper.hpp"
#include "oge/runtime/asset_ctx.hpp"
#include "oge/runtime/gfx/draw_context.hpp"
#include "oge/runtime/gfx/ui_pass.hpp"
#include "oge/runtime/typed_registry.hpp"

namespace game
{
using game::view::gfx::DebugInfoPass;
using game::view::gfx::TerrainPass2;
using oge::input::RawInputStream;
using oge::runtime::AnythingFactory;
using oge::runtime::AssetContext;
using oge::runtime::OGEContext;
using oge::runtime::gfx::UIPass;

class GraphicalScene : protected AppRuntime
{
    using ViewExecutor =
        view::ViewExecutor<TerrainPass2, UIPass, DebugInfoPass>;
    struct Ctx : AppContext
    {
        OGEContext& ctx;
        AssetContext assets;

        Ctx(AppContext actx, OGEContext& ctx) : AppContext(actx), ctx(ctx), assets(ctx) {}
    };

   protected:
    entt::registry m_world;

    sim::SubsystemPipeline m_subsystems;
    sim::RealtimeSubsystemPipeline m_realtimeSubsystems;

   public:
    struct Frame
    {
        float dt;
        oge::input::RawInputStream& is;
        FramePerfStatus perfStats;
        AppFrameAction& frameAction;
    };

    class Instance : protected AppRuntime
    {
       protected:
        entt::registry& m_world;
        sim::SubsystemPipeline& m_subsystems;
        sim::RealtimeSubsystemPipeline& m_realtimeSubsystems;
        Ctx m_ctx;

        input::InputPipeline m_inputs;
        WindowCtx m_windowCtx;

        entt::registry m_renderWorld;
        view::RenderPipeline m_renderers;

        view::SubmissionQueue m_squeue;
        ViewExecutor m_viewExecutor;

       public:
        Instance(GraphicalScene& scene, const json::Value& args,
                 OGEContext& ctx)
            : AppRuntime(scene),
              m_ctx(scene.m_ctx, ctx),
              m_world(scene.m_world),
              m_subsystems(scene.m_subsystems),
              m_realtimeSubsystems(scene.m_realtimeSubsystems),
              m_inputs(input::InputContext{m_windowCtx, scene.m_world}),
              m_renderers(view::RendererState{scene.m_world, m_renderWorld,
                                              m_ctx.events, m_ctx.memory,
                                              AssetContext(ctx)}),
              m_squeue(m_ctx.memory.frameBuffer.Resource())
        {
            m_viewExecutor.Attach(ctx);
        }

        virtual ~Instance() { m_viewExecutor.Detach(); }

        virtual void Update(Frame f)
        {
            m_ctx.memory.Update(f.dt);
            m_inputs.Update({f.dt, f.is});
            m_world.ctx().insert_or_assign(f.perfStats);
            m_subsystems.Update(f.dt);
            m_realtimeSubsystems.Update(f.dt);

            m_squeue.Clear();
            m_renderers.Update(view::RendererFrameData{
                f.dt, m_ctx.assets, m_squeue, m_subsystems.GetAlpha()});
            f.frameAction |= m_windowCtx.frameAction;
            m_windowCtx.Clear();
        }

        ViewExecutor& GetPasses() { return m_viewExecutor; }

        void Render(float dt) { m_viewExecutor.Update(dt, m_squeue); }
    };

    GraphicalScene(AppContext ctx)
        : AppRuntime(ctx),
          m_subsystems({m_world, ctx.events, ctx.memory}, 1.f / 30.f),
          m_realtimeSubsystems({m_world, ctx.events, ctx.memory})
    {
    }

    virtual ~GraphicalScene() {}

    virtual std::unique_ptr<Instance> Attach(const json::Value& args,
                                             OGEContext& ctx)
    {
        return std::make_unique<Instance>(*this, args, ctx);
    }

    virtual void Load(SceneConfig&& config)
    {
        for (auto stage : config.subsystems)
        {
            m_subsystems.AddStage(m_ctx.any_factory, stage);
        }
        for (auto stage : config.realtimeSubsystems)
        {
            m_realtimeSubsystems.AddStage(m_ctx.any_factory, stage);
        }
    }

    virtual void Unload()
    {
        m_world.ctx().clear();
        m_subsystems.Clear();
        m_realtimeSubsystems.Clear();
    }
};

}  // namespace game
