#pragma once
#include <algorithm>
#include <concepts>

#include "entt/core/fwd.hpp"
#include "game/app_context.hpp"
#include "game/frame_perf.hpp"
#include "oge/input/raw_input_stream.hpp"
#include "oge/runtime/typed_registry.hpp"
#include "game/json.hpp"

namespace game
{
using oge::runtime::OGEContext;

template <typename T>
concept IsScene =
    requires(T s, float dt, const json::Value& args, T::Frame f, OGEContext& rctx, AnythingFactory& af) {
        typename T::Frame;
        std::constructible_from<T, AppContext>;
        { s.Attach(args, rctx, af) };
        { s.Update(f) };
        { s.Detach() };
    };

template <typename TSceneBase>
    requires IsScene<TSceneBase>
class SceneRunner
{
   public:
    SceneRunner(OGEContext& ctx)
        : m_ctx(ctx), m_anyFactory(ctx), m_appCtx(m_anyFactory, m_events)
    {
        RegisterScene<TSceneBase>();
        SwitchToScene<TSceneBase>();
    }

    template <typename TScene>
        requires std::derived_from<TScene, TSceneBase>
    void RegisterScene()
    {
        m_scenes.emplace(std::type_index(typeid(TScene)),
                         std::make_unique<TScene>(m_appCtx));
    }

    template <typename TScene>
    void SwitchToScene(json::Value sceneArgs = nullptr)
    {
        m_nextSceneArgs = std::move(sceneArgs);
        m_nextScene =
            m_scenes.find(std::type_index(typeid(TScene)))->second.get();
    }

   protected:
    TSceneBase* CurrentScene() { return m_currentScene; }

    void UpdateScene(TSceneBase::Frame f)
    {
        if (m_nextScene != nullptr)
        {
            if (m_currentScene != nullptr)
            {
                m_currentScene->Detach();
            }
            m_nextScene->Attach(m_nextSceneArgs, m_ctx, m_anyFactory);
            m_currentScene = m_nextScene;
            m_nextScene = nullptr;
        }
        m_currentScene->Update(std::move(f));
    }

    void DetachScene()
    {
        if (m_currentScene == nullptr) return;
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

    std::unordered_map<std::type_index, std::unique_ptr<TSceneBase>> m_scenes;
    json::Value m_nextSceneArgs = nullptr;
    TSceneBase* m_nextScene = nullptr;
    TSceneBase* m_currentScene = nullptr;
};
}  // namespace game