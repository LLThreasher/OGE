#pragma once
#include <algorithm>
#include <concepts>
#include <memory>
#include <optional>
#include <utility>

#include "game/app_context.hpp"
#include "game/frame_perf.hpp"
#include "game/json.hpp"
#include "game/memory_context.hpp"
#include "oge/input/raw_input_stream.hpp"
#include "oge/log.hpp"
#include "oge/runtime/typed_registry.hpp"

namespace game
{
using oge::runtime::oge_id_type;
using oge::runtime::OGEContext;

struct SceneContext
{
    std::optional<oge_id_type>& nextScene;
    json::Object& nextSceneArgs;
};

template <typename T>
concept IsScene = requires(T s, T::Frame f, SceneContext ctx) {
    typename T::Frame;
    std::constructible_from<T, AppContext, const json::Object&, OGEContext&>;
    { s.Update(f, ctx) };
};

template <typename TSceneBase>
    requires IsScene<TSceneBase>
class SceneRunner
{
   public:
    SceneRunner()
        : m_ctx(m_metaWorld.Raw()),
          m_anyFactory(m_ctx),
          m_appCtx(m_ctx, m_anyFactory, m_events, m_memory)
    {
        m_anyFactory.RegisterABC<TSceneBase>();
        RegisterScene<TSceneBase>();
        SwitchToScene<TSceneBase>();
    }

    template <typename TScene>
        requires std::derived_from<TScene, TSceneBase>
    void RegisterScene()
    {
        m_anyFactory.RegisterDerived<TSceneBase, TScene>();
    }

    template <typename TScene>
    void SwitchToScene(json::Object sceneArgs = {})
    {
        m_nextSceneArgs = std::move(sceneArgs);
        m_nextScene = m_anyFactory.Id<TScene>();
    }

    void SwitchToScene(std::string_view sceneName, json::Object sceneArgs = {})
    {
        m_nextSceneArgs = std::move(sceneArgs);
        m_nextScene = m_anyFactory.Id(sceneName);
    }

    template <typename T>
    oge_id_type Id()
    {
        return entt::type_hash<T>::value();
    }

    AnythingFactory& AF()
    {
        return m_anyFactory;
    }

   protected:
    TSceneBase* CurrentScene()
    {
        return m_currentScene.get();
    }

    void UpdateScene(TSceneBase::Frame f)
    {
        if (m_nextScene.has_value())
        {
            if (m_currentScene != nullptr)
            {
                m_currentScene.reset();
            }
            auto nextScene = m_anyFactory.BuildABC<TSceneBase>(
                m_nextScene.value(),
                typename TSceneBase::Def{m_appCtx, m_nextSceneArgs});
            if (nextScene == nullptr)
            {
                if (auto desc = m_anyFactory.GetDescriptor(m_nextScene.value()))
                {
                    LOG_ERROR("cannot switch to scene with name {}",
                              desc->name);
                }
                else
                {
                    LOG_ERROR("cannot switch to unregistered scene with id {}",
                              m_nextScene.value());
                }
                SwitchToScene<TSceneBase>();
                m_nextSceneArgs = {};
                nextScene = m_anyFactory.BuildABC<TSceneBase>(
                    Id<TSceneBase>(),
                    typename TSceneBase::Def{m_appCtx, m_nextSceneArgs});
                assert(nextScene != nullptr);
            }
            LOG_INFO("switched to scene with name {}",
                        m_anyFactory.GetDescriptor(m_nextScene.value())->name);
            m_currentScene = std::move(nextScene);
            m_nextScene.reset();
        }
        m_currentScene->Update(std::forward<typename TSceneBase::Frame>(f),
                               SceneContext{m_nextScene, m_nextSceneArgs});
    }

    void DetachScene()
    {
        if (m_currentScene == nullptr) return;
        m_nextScene = m_currentSceneId;
        m_currentScene.reset();
        m_currentSceneId.reset();
    }

   protected:
    oge::runtime::OgeRegistry m_metaWorld;
    OGEContext m_ctx;
    AnythingFactory m_anyFactory;
    entt::dispatcher m_events;
    MemoryContext m_memory = {
        {1 * 1024 * 1024}, {1 * 1024 * 1024, 10.f}, {1 * 1024 * 1024, 0.2f}};

   private:
    AppContext m_appCtx;

    json::Object m_nextSceneArgs = {};
    std::optional<oge_id_type> m_nextScene;
    std::optional<oge_id_type> m_currentSceneId;
    std::unique_ptr<TSceneBase> m_currentScene = nullptr;
};
}  // namespace game