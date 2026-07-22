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
    MemoryContext m_memory{{MAX_FRAMES_IN_FLIGHT, 1 * 1024 * 1024}, {1 * 1024 * 1024, 5.f}};
    std::array<view::SubmissionQueue, 3> m_squeue{m_memory.frameBuffer.Get(0),
                                                  m_memory.frameBuffer.Get(1),
                                                  m_memory.frameBuffer.Get(2)};
    ViewExecutor m_viewExecutor;
    std::optional<Ctx> m_ctx;

    sim::GameState m_gameState;
    input::InputContext m_inputState;
    WindowCtx m_windowCtx;

    entt::registry m_world;
    sim::SubsystemPipeline m_subsystems;
    sim::RealtimeSubsystemPipeline m_realtimeSubsystems;
    input::InputPipeline m_inputs;

    entt::registry m_renderWorld;
    std::optional<view::RendererState> m_renderState;
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
        : m_gameState(m_world, ctx.events, m_memory),
          m_subsystems(m_gameState, ctx.any_factory, 1.f / 30.f),
          m_realtimeSubsystems(m_gameState, ctx.any_factory),
          m_inputState(m_windowCtx, m_world),
          m_inputs(m_inputState, ctx.any_factory)
    {
    }

    virtual ~GraphicalScene() {}

    virtual void Attach(const json::Value& args, OGEContext& ctx,
                        AnythingFactory& af)
    {
        m_renderState.emplace(m_world, m_renderWorld, m_gameState.events,
                              m_memory, AssetContext(ctx));
        m_renderers.emplace(*&m_renderState.value(), af);
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
        m_inputs.Update({f.dt, f.is});
        m_world.ctx().insert_or_assign(f.perfStats);
        m_subsystems.Update(f.dt);
        m_realtimeSubsystems.Update(f.dt);

        m_squeue[m_memory.frameBuffer.Idx()].Clear();
        m_renderers->Update(view::RendererFrameData{
            f.dt, m_ctx.value().assets, m_squeue[m_memory.frameBuffer.Idx()],
            m_subsystems.GetAlpha()});
        f.frameAction |= m_windowCtx.frameAction;
        m_windowCtx.Clear();
    }

    void Render(float dt)
    {
        m_viewExecutor.Update(dt, m_squeue[m_memory.frameBuffer.Idx()]);
    }
};

}  // namespace game
