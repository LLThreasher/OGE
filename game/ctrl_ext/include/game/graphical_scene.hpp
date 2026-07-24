#pragma once

#include <array>
#include <memory>
#include <memory_resource>
#include <optional>
#include <valarray>
#include <vector>

#include "game/app_context.hpp"
#include "game/input/input_source.hpp"
#include "game/json.hpp"
#include "game/memory_context.hpp"
#include "game/sim/subsystem.hpp"
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

class GraphicalScene
{
    using ViewExecutor =
        view::ViewExecutor<TerrainPass2, UIPass, DebugInfoPass>;
    struct Ctx
    {
        OGEContext& ctx;
        AssetContext assets;

        Ctx(OGEContext& ctx) : ctx(ctx), assets(ctx) {}
    };

   protected:
    MemoryContext m_memory{{1 * 1024 * 1024}, {1 * 1024 * 1024, 5.f}};
    view::SubmissionQueue m_squeue{m_memory.frameBuffer.Resource()};
    ViewExecutor m_viewExecutor;
    std::optional<Ctx> m_ctx;

    WindowCtx m_windowCtx;

    entt::registry m_world;
    entt::dispatcher& m_events;

    sim::SubsystemPipeline m_subsystems;
    sim::RealtimeSubsystemPipeline m_realtimeSubsystems;
    std::optional<input::InputPipeline> m_inputs;

    entt::registry m_renderWorld;
    std::optional<view::RenderPipeline> m_renderers;

    ViewExecutor& GetPasses() { return m_viewExecutor; }

   public:
    struct Frame
    {
        float dt;
        oge::input::RawInputStream& is;
        FramePerfStatus perfStats;
        AppFrameAction& frameAction;
    };

    GraphicalScene(AppContext ctx)
        : m_events(ctx.events),
          m_subsystems({m_world, m_events, m_memory}, ctx.any_factory, 1.f / 30.f),
          m_realtimeSubsystems({m_world, m_events, m_memory}, ctx.any_factory)
    {
    }

    virtual ~GraphicalScene() {}

    virtual void Attach(const json::Value& args, OGEContext& ctx,
                        AnythingFactory& af)
    {
        m_inputs.emplace(input::InputContext{m_windowCtx, m_world}, af);
        m_renderers.emplace(view::RendererState{m_world, m_renderWorld, m_events,
                              m_memory, AssetContext(ctx)}, af);
        m_ctx.emplace(ctx);
        m_viewExecutor.Attach(ctx);
    }

    virtual void Detach()
    {
        m_viewExecutor.Detach();
        m_ctx.reset();
    }

    virtual void Update(Frame f)
    {
        m_memory.Update(f.dt);
        m_inputs.value().Update({f.dt, f.is});
        m_world.ctx().insert_or_assign(f.perfStats);
        m_subsystems.Update(f.dt);
        m_realtimeSubsystems.Update(f.dt);

        m_squeue.Clear();
        m_renderers->Update(view::RendererFrameData{
            f.dt, m_ctx.value().assets, m_squeue,
            m_subsystems.GetAlpha()});
        f.frameAction |= m_windowCtx.frameAction;
        m_windowCtx.Clear();
    }

    void Render(float dt)
    {
        m_viewExecutor.Update(dt, m_squeue);
    }
};

}  // namespace game
