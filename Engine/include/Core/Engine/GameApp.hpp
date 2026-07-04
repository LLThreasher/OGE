#pragma once
#include <memory>
#include <string>

#include "Engine/AssetManager.hpp"
#include "Engine/Input/InputSystem.hpp"
#include "Engine/IServerScene.hpp"
#include "Engine/TickScheduler.hpp"
#include "Engine/entt.hpp"

namespace OneGame::Engine
{
namespace Graphics
{
class WindowHandle;
class IGraphicsBackend;
}  // namespace Graphics

class GameApp
{
   public:
    void Initialize();
    AppFrameAction Update(float dt);
    void Shutdown();

   private:
    AssetManager assetManager;
    entt::dispatcher dispatcher;

    std::unique_ptr<ServerSceneBase> m_serverScene;
};
}  // namespace OneGame::Engine
