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
using oge::runtime::oge_id_type;
using oge::runtime::OGEContext;

template <typename T>
concept IsScene = requires(T s, T::Frame f) {
    typename T::Frame;
    std::constructible_from<T, AppContext, const json::Value&, OGEContext&>;
    { s.Update(f) };
};

template <typename TSceneBase>
    requires IsScene<TSceneBase>
class SceneRunner
{
   public:
    SceneRunner()
        : m_ctx(m_metaWorld),
          m_anyFactory(m_ctx),
          m_appCtx(m_anyFactory, m_events, m_memory)
    {
        m_anyFactory.RegisterABC<TSceneBase>();
        RegisterScene<TSceneBase>();
        SwitchToScene<TSceneBase>();
    }

    template <typename TScene>
        requires std::derived_from<TScene, TSceneBase>
    void RegisterScene()
    {
        m_anyFactory.RegisterDrived<TSceneBase, TScene>();
    }

    template <typename TScene>
    void SwitchToScene(json::Value sceneArgs = nullptr)
    {
        m_nextSceneArgs = std::move(sceneArgs);
        m_nextScene = m_anyFactory.Id<TScene>();
    }

   protected:
    TSceneBase* CurrentScene() { return m_currentScene.get(); }

    void UpdateScene(TSceneBase::Frame f)
    {
        if (m_nextScene.has_value())
        {
            if (m_currentScene != nullptr)
            {
                m_currentScene.reset();
            }
            m_currentScene =
                m_anyFactory.BuildABC<TSceneBase>(m_nextScene.value(), typename TSceneBase::Def{m_appCtx, m_nextSceneArgs, m_ctx});
            m_nextScene.reset();
        }
        m_currentScene->Update(std::forward<typename TSceneBase::Frame>(f));
    }

    void DetachScene()
    {
        if (m_currentScene == nullptr) return;
        m_nextScene = m_currentSceneId;
        m_currentScene.reset();
        m_currentSceneId.reset();
    }

   protected:
    entt::registry m_metaWorld;
    OGEContext m_ctx;
    AnythingFactory m_anyFactory;
    entt::dispatcher m_events;
    MemoryContext m_memory = {
        {1 * 1024 * 1024}, {1 * 1024 * 1024, 10.f}, {1 * 1024 * 1024, 0.2f}};

   private:
    AppContext m_appCtx;

    json::Value m_nextSceneArgs = nullptr;
    std::optional<oge_id_type> m_nextScene;
    std::optional<oge_id_type> m_currentSceneId;
    std::unique_ptr<TSceneBase> m_currentScene = nullptr;
};
}  // namespace game