#pragma once
#include <memory>
#include <string>
#include <typeindex>
#include <typeinfo>
#include <unordered_map>

#include "Engine/AssetManager.hpp"
#include "Engine/GameAppState.hpp"
#include "Engine/IScene.hpp"
#include "Engine/Input/InputSystem.hpp"
#include "Engine/TickScheduler.hpp"
#include "Engine/entt.hpp"

namespace OneGame::Engine
{

using HeadlessScene = Scene<AppContext, const FrameData>;

class GameHeadlessApp : public SceneRunner<AppContext, const FrameData>
{
    using Parent = SceneRunner<AppContext, const FrameData>;

   public:
    GameHeadlessApp() : m_appContext({m_assetManager}, {m_dispatcher, m_sceneArgs}) {}
    void Initialize();
    bool Update(float dt);
    bool HandleCmd(std::string_view cmd);
    void Shutdown();

   protected:
    bool m_isrunning = true;

    entt::meta_any m_sceneArgs;

    AssetManager m_assetManager;
    entt::dispatcher m_dispatcher;
    AppContext m_appContext;

    SceneRunner<AppContext> m_scenes;

    std::unordered_map<std::string, std::function<bool(std::string_view)>> m_cmdRunners;
};
}  // namespace OneGame::Engine
