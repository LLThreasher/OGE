#pragma once
#include <algorithm>
#include <concepts>
#include <memory>
#include <utility>

#include "game/app_context.hpp"
#include "game/frame_perf.hpp"
#include "game/json.hpp"
#include "game/memory_context.hpp"
#include "oge/input/raw_input_stream.hpp"
#include "oge/runtime/typed_registry.hpp"

namespace game
{
using oge::runtime::OGEContext;

template <typename T>
concept IsScene =
    requires(T s, T::Instance ss, const json::Value& args, T::Frame f,
             OGEContext& rctx) {
        typename T::Frame;
        typename T::Instance;
        std::constructible_from<T, AppContext>;
        { s.Attach(args, rctx) } -> std::same_as<std::unique_ptr<typename T::Instance>>;
        { ss.Update(f) };
    };

template <typename TSceneBase>
    requires IsScene<TSceneBase>
class SceneRunner
{
   public:
    SceneRunner(OGEContext& ctx)
        : m_ctx(ctx),
          m_anyFactory(ctx),
          m_appCtx(m_anyFactory, m_events, m_memory)
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
    TSceneBase::Instance* CurrentScene() { return m_currentSceneInstance.get(); }

    void UpdateScene(TSceneBase::Frame f)
    {
        if (m_nextScene != nullptr)
        {
            if (m_currentScene != nullptr)
            {
                m_currentSceneInstance.reset();
            }
            m_currentSceneInstance = std::move(m_nextScene->Attach(m_nextSceneArgs, m_ctx));
            m_currentScene = m_nextScene;
            m_nextScene = nullptr;
        }
        m_currentSceneInstance->Update(std::forward<typename TSceneBase::Frame>(f));
    }

    void DetachScene()
    {
        if (m_currentScene == nullptr) return;
        m_currentSceneInstance.reset();
        m_nextScene = m_currentScene;
        m_currentScene = nullptr;
    }

   protected:
    AnythingFactory m_anyFactory;
    entt::dispatcher m_events;
    MemoryContext m_memory = {
        {1 * 1024 * 1024}, {1 * 1024 * 1024, 10.f}, {1 * 1024 * 1024, 0.2f}};

   private:
    OGEContext& m_ctx;
    AppContext m_appCtx;

    std::unordered_map<std::type_index, std::unique_ptr<TSceneBase>> m_scenes;
    json::Value m_nextSceneArgs = nullptr;
    TSceneBase* m_nextScene = nullptr;
    TSceneBase* m_currentScene = nullptr;
    std::unique_ptr<typename TSceneBase::Instance> m_currentSceneInstance;
};
}  // namespace game