#pragma once

#include "game/app_context.hpp"
#include "game/input/input_source.hpp"
#include "game/json.hpp"
#include "game/memory_context.hpp"
#include "game/scene.hpp"
#include "game/sim/subsystem.hpp"
#include "game/view/gfx/debug_info_pass.hpp"
#include "game/view/gfx/terrain_pass2.hpp"
#include "game/view/gfx/view_executor.hpp"
#include "game/view/renderer.hpp"
#include "game/view/submission_queue.hpp"
#include "oge/runtime/asset_ctx.hpp"
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

class SceneExt : public Scene
{
    using ViewExecutor =
        view::ViewExecutor<TerrainPass2, UIPass, DebugInfoPass>;
    struct Ctx : AppContext
    {
        OGEContext& ctx;
        AssetContext assets;

        Ctx(AppContext actx, OGEContext& ctx)
            : AppContext(actx), ctx(ctx), assets(ctx)
        {
        }
    };

   public:
    struct Frame
    {
        float dt;
        oge::input::RawInputStream& is;
        FramePerfStatus perfStats;
        AppFrameAction& frameAction;
    };

    Ctx m_ctx;

    input::InputPipeline m_inputs;
    WindowCtx m_windowCtx;

    entt::registry m_renderWorld;
    view::RenderPipeline m_renderers;

    view::SubmissionQueue m_squeue;
    ViewExecutor m_viewExecutor;

    SceneExt(const Def& def)
        : Scene(def),
            m_ctx(def.ctx, def.rctx),
            m_inputs(input::InputContext{m_windowCtx, m_world}),
            m_renderers(view::RendererState{m_world, m_renderWorld,
                                            m_ctx.events, m_ctx.memory,
                                            AssetContext(def.rctx)}),
            m_squeue(m_ctx.memory.frameBuffer.Resource())
    {
        m_viewExecutor.Attach(def.rctx);
    }

    virtual ~SceneExt() { m_viewExecutor.Detach(); }

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
}  // namespace game
