#pragma once

#include <memory>
#include <optional>

#include "game/app_context.hpp"
#include "game/input/input_source.hpp"
#include "game/sim/subsystem.hpp"
#include "game/view/gfx/debug_info_pass.hpp"
#include "game/view/gfx/terrain_pass.hpp"
#include "game/view/gfx/view_executor.hpp"
#include "game/view/renderer.hpp"
#include "game/view/submission_queue.hpp"
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

struct SceneFrame
{
    float dt;
    oge::input::RawInputStream& is;
    FramePerfStatus perfStats;
    AppFrameAction& frameAction;
};

class Scene
{
    using ViewExecutor = view::ViewExecutor<view::SubmissionQueue, TerrainPass2, UIPass, DebugInfoPass>;
    struct Ctx
    {
        OGEContext& ctx;
        AssetContext assets;
        view::SubmissionQueue s_queue;
        ViewExecutor viewExecutor;

        Ctx(OGEContext& ctx) : ctx(ctx), assets(ctx), viewExecutor(s_queue) { viewExecutor.Attach(ctx); }

        ~Ctx() { viewExecutor.Detach(); }
    };

   protected:
    std::optional<Ctx> m_ctx;

    sim::GameState m_gameState;
    input::InputContext m_inputState;
    WindowCtx m_windowCtx;

    entt::registry m_world;
    sim::SubsystemPipeline m_subsystems;
    input::InputPipeline m_inputs;

    entt::registry m_renderWorld;
    std::optional<view::RendererState> m_renderState;
    std::optional<view::RenderPipeline> m_renderers;

    ViewExecutor& GetPasses()
    {
        return m_ctx.value().viewExecutor;
    }

   public:
    Scene(AppContext ctx)
        : m_gameState(m_world, ctx.events),
          m_subsystems(m_gameState, ctx.any_factory, 1.f / 30.f),
          m_inputState(m_windowCtx, m_world),
          m_inputs(m_inputState, ctx.any_factory)
    {
    }

    virtual ~Scene() {}

    virtual void Attach(OGEContext& ctx, AnythingFactory& af) { 
        m_renderState.emplace(m_world, m_renderWorld, m_gameState.events, AssetContext(ctx));
        m_renderers.emplace(*&m_renderState.value(), af);
        m_ctx.emplace(ctx); }

    virtual void Detach() { m_ctx.reset(); }

    virtual void Update(SceneFrame f)
    {
        m_inputs.Update({f.dt, f.is});
        m_world.ctx().insert_or_assign(f.perfStats);
        m_subsystems.Update(f.dt);
        m_renderers->Update(view::RendererFrameData{f.dt, m_ctx.value().assets, m_ctx.value().s_queue, m_subsystems.GetAlpha()});
        f.frameAction |= m_windowCtx.frameAction;
        m_windowCtx.Clear();
    }

    void Render(float dt) { m_ctx.value().viewExecutor.Update(dt); }
};

class SceneRunner
{
   public:
    SceneRunner(OGEContext& ctx)
        : m_ctx(ctx),
        m_anyFactory(ctx),
        m_appCtx(m_anyFactory, m_events)
    {
        RegisterScene<Scene>();
        SwitchToScene<Scene>();
    }

    template <typename TScene>
        requires std::derived_from<TScene, Scene>
    void RegisterScene()
    {
        m_scenes.emplace(std::type_index(typeid(TScene)), std::make_unique<TScene>(m_appCtx));
    }

    template <typename TScene>
    void SwitchToScene()
    {
        m_nextScene = m_scenes.find(std::type_index(typeid(TScene)))->second.get();
    }

   protected:
    void UpdateScene(SceneFrame f)
    {
        if (m_nextScene != nullptr)
        {
            if (m_currentScene != nullptr)
            {
                m_currentScene->Detach();
            }
            m_nextScene->Attach(m_ctx, m_anyFactory);
            m_currentScene = m_nextScene;
            m_nextScene = nullptr;
        }
        m_currentScene->Update(std::move(f));
    }

    void RenderScene(float dt) { m_currentScene->Render(dt); }

    void DetachScene()
    {
        m_currentScene->Detach();
        m_nextScene = m_currentScene;
        m_currentScene = nullptr;
    }

   protected:
    AnythingFactory m_anyFactory;
    entt::dispatcher m_events;

   private:
    OGEContext& m_ctx;
    AppContext m_appCtx;

    std::unordered_map<std::type_index, std::unique_ptr<Scene>> m_scenes;
    Scene* m_nextScene = nullptr;
    Scene* m_currentScene = nullptr;
};
}  // namespace game
