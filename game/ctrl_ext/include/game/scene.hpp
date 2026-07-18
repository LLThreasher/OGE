#pragma once

#include "game/input/input_srouce.hpp"
#include "game/sim/subsystem.hpp"
#include "game/view/gfx/debug_info_pass.hpp"
#include "game/view/gfx/terrain_pass.hpp"
#include "game/view/gfx/view_executor.hpp"
#include "game/view/renderer.hpp"
#include "game/view/submission_queue.hpp"
#include "oge/runtime/asset_ctx.hpp"
#include "oge/runtime/gfx/draw_context.hpp"
#include "oge/runtime/gfx/ui_pass.hpp"

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

    std::optional<Ctx> m_ctx;

    entt::registry m_world;
    entt::dispatcher m_event;
    input::PlayerInputStream m_playerIn;

    sim::GameState m_gameState;
    view::RendererState m_renderState;
    input::InputContext m_inputState;

    WindowCtx m_windowCtx;

   protected:
    sim::SubsystemPipeline m_subsystems;
    view::RenderPipeline m_renderers;
    input::InputPipeline m_inputs;

   public:
    Scene(AnythingFactory& af)
        : m_gameState(m_world, m_event),
          m_renderState(m_world, m_event),
          m_subsystems(m_gameState, af),
          m_renderers(m_renderState, af),
          m_inputState(m_windowCtx, m_playerIn, m_world),
          m_inputs(m_inputState, af)
    {
    }

    virtual ~Scene() {}

    virtual void Attach(OGEContext& ctx) { m_ctx.emplace(ctx); }

    virtual void Detach() { m_ctx.reset(); }

    virtual void Update(SceneFrame f)
    {
        m_inputs.Update({f.dt, f.is});
        m_world.ctx().insert_or_assign(f.perfStats);
        m_subsystems.Update(f.dt);
        m_renderers.Update(view::RendererFrameData{f.dt, m_ctx.value().assets, m_ctx.value().s_queue});
        m_ctx.value().viewExecutor.Update(f.dt);
    }
};

class SceneRunner
{
   public:
    SceneRunner(OGEContext& ctx) : m_ctx(ctx), m_anyFactory(ctx)
    {
        m_scenes.emplace(std::type_index(typeid(Scene)), new Scene(m_anyFactory));
        SwitchToScene<Scene>();
    }

    template <typename TScene>
        requires std::derived_from<TScene, Scene>
    void RegisterScene()
    {
        m_scenes.emplace(std::type_index(typeid(TScene)), std::make_unique<TScene>(m_anyFactory));
    }

    template <typename TScene>
    void SwitchToScene()
    {
        m_nextScene = m_scenes.find(std::type_index(typeid(TScene)))->second.get();
    }

   protected:
    void Update(SceneFrame f)
    {
        if (m_nextScene != nullptr)
        {
            if (m_currentScene != nullptr)
            {
                m_currentScene->Detach();
            }
            m_nextScene->Attach(m_ctx);
            m_currentScene = m_nextScene;
            m_nextScene = nullptr;
        }
        m_currentScene->Update(std::move(f));
    }

    void DetachCurrentScene()
    {
        m_currentScene->Detach();
        m_nextScene = m_currentScene;
        m_currentScene = nullptr;
    }

   protected:
    AnythingFactory m_anyFactory;

   private:
    OGEContext& m_ctx;

    std::unordered_map<std::type_index, std::unique_ptr<Scene>> m_scenes;
    Scene* m_nextScene = nullptr;
    Scene* m_currentScene = nullptr;
};
}  // namespace game
