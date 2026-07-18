#pragma once

#include "game/sim/subsystem.hpp"
#include "game/view/renderer.hpp"
#include "game/view/submission_queue.hpp"
#include "oge/runtime/asset_ctx.hpp"
#include "oge/runtime/gfx/draw_context.hpp"
#include "game/input/input_srouce.hpp"
#include "game/view/gfx/view_executor.hpp"

namespace game
{
using oge::runtime::OGEContext;
using oge::runtime::AssetContext;
using oge::runtime::AnythingFactory;
using oge::input::RawInputStream;

class Scene
{
    struct Ctx
    {
        OGEContext& ctx;
        AssetContext assets;
        view::SubmissionQueue s_queue;
        view::ViewExecutor<view::SubmissionQueue> viewExecutor;

        Ctx(OGEContext& ctx) : ctx(ctx), assets(ctx), viewExecutor(s_queue)
        {
            viewExecutor.Attach(ctx);
        }
        
        ~Ctx()
        {
            viewExecutor.Detach();
        }
    };

    std::optional<Ctx> m_ctx;
    sim::SubsystemPipeline m_subsystems;
    view::RenderPipeline m_renderers;
    input::InputPipeline m_inputs;

    entt::registry m_world;
    entt::dispatcher m_event;
    input::PlayerInputStream m_playerIn;

    sim::GameState m_gameState;
    view::RendererState m_renderState;
    input::InputContext m_inputState;

    WindowCtx m_windowCtx;

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

    virtual ~Scene()
    {
    }

    void Attach(OGEContext& ctx) { m_ctx.emplace(ctx); }

    void Detach() { m_ctx.reset(); }

    void Update(float dt, oge::input::RawInputStream& is)
    {
        m_inputs.Update({dt, is});
        m_subsystems.Update(dt);
        m_renderers.Update(view::RendererFrameData{dt, m_ctx.value().assets, m_ctx.value().s_queue});
        m_ctx.value().viewExecutor.Update(dt);
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
    void RegisterScene()
    {
        m_scenes.emplace(std::type_index(typeid(TScene)), new TScene(m_anyFactory));
    }

    template <typename TScene>
    void SwitchToScene()
    {
        m_nextScene = m_scenes.find(std::type_index(typeid(TScene)))->second.get();
    }

   protected:
    void Update(float dt, oge::input::RawInputStream& is)
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
        m_currentScene->Update(dt, is);
    }

   private:
    OGEContext& m_ctx;
    AnythingFactory m_anyFactory;

    std::unordered_map<std::type_index, std::unique_ptr<Scene>> m_scenes;
    Scene* m_nextScene = nullptr;
    Scene* m_currentScene = nullptr;
};
}  // namespace game
