#include "game/scene_ext.hpp"

using namespace game;

SceneExt::SceneExt(const Def& def)
    : Scene(def),
      m_ctx(def.ctx),
      m_inputs(input::InputContext{m_windowCtx, m_world}),
      m_renderers(view::RendererState{m_world, m_renderWorld, m_ctx.events,
                                      m_ctx.memory, AssetContext(def.ctx.any_ctx)}),
      m_squeue(m_ctx.memory.frameBuffer.Resource())
{
    m_viewExecutor.Attach(def.ctx.any_ctx);
}

SceneExt::~SceneExt() { m_viewExecutor.Detach(); }

void SceneExt::Update(Frame f, SceneContext sctx)
{
    // input processing
    m_inputs.Update({f.dt, f.is});
    m_world.ctx().insert_or_assign(f.perfStats);

    // simulation
    Scene::Update(f, sctx);

    // presentation
    m_squeue.Clear();
    m_renderers.Update(view::RendererFrameData{f.dt, m_ctx.assets, m_squeue,
                                               m_subsystems.GetAlpha()});

    // window action
    f.frameAction |= m_windowCtx.frameAction;
    m_windowCtx.Clear();
}

SceneExt::ViewExecutor& SceneExt::GetPasses() { return m_viewExecutor; }

void SceneExt::Render(float dt) { m_viewExecutor.Update(dt, m_squeue); }