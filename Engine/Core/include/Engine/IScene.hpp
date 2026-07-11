#pragma once

#include <memory>
#include <string>
#include <typeindex>
#include <unordered_map>

namespace OneGame::Engine
{

template <typename TData, typename... TFrame>
class Scene
{
   public:
    virtual void Initialize(TData& context) {}
    virtual void Enter(TData& context) {}
    virtual void Exit(TData& context) {}
    virtual void Shutdown(TData& context) {}
    virtual void Update(TData& context, TFrame&... frame) {}
};

template <typename TData, typename... TFrame>
class SceneRunner
{
    using SceneBase = Scene<TData, TFrame...>;

   public:
    SceneRunner()
    {
        m_scenes.emplace(std::type_index(typeid(SceneBase)), new SceneBase());
        SwitchToScene<SceneBase>();
    }

    template <typename TScene>
    void RegisterScene()
    {
        m_scenes.emplace(std::type_index(typeid(TScene)), new TScene());
    }

    template <typename TScene>
    void SwitchToScene()
    {
        m_nextScene = m_scenes.find(std::type_index(typeid(TScene)))->second.get();
    }

   protected:
    void Initialize(TData& ctx)
    {
        for (auto& [key, val] : m_scenes)
        {
            val->Initialize(ctx);
        }
    }

    void Shutdown(TData& ctx)
    {
        for (auto& [key, val] : m_scenes)
        {
            val->Shutdown(ctx);
        }
    }

    void Update(TData& ctx, TFrame&... inputs)
    {
        if (m_nextScene != nullptr)
        {
            if (m_currentScene != nullptr)
            {
                m_currentScene->Exit(ctx);
            }
            m_nextScene->Enter(ctx);
            m_currentScene = m_nextScene;
            m_nextScene = nullptr;
        }
        m_currentScene->Update(ctx, inputs...);
    }

   private:
    std::unordered_map<std::type_index, std::unique_ptr<SceneBase>> m_scenes;
    SceneBase* m_nextScene = nullptr;
    SceneBase* m_currentScene = nullptr;
};

}  // namespace OneGame::Engine
