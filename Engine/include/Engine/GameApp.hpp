#pragma once
#include <memory>
#include <string>

#include "Engine/AssetManager.hpp"
#include "Engine/Graphics/Renderer.hpp"
#include "Engine/Input/InputSystem.hpp"
#include "Engine/Scenes/IScene.hpp"
#include "Engine/StreamingManager.hpp"
#include "Engine/TickScheduler.hpp"
#include "Engine/entt.hpp"

namespace OneGame::Engine
{
namespace Graphics
{
class WindowHandle;
class IGraphicsBackend;
}  // namespace Graphics

class GameServerApp
{
   public:
    void Initialize();
    void Update(float dt);
    void Shutdown();

   private:
    AssetManager assetManager;
    entt::dispatcher dispatcher;

    std::unique_ptr<IServerScene> m_serverScene;
};

class GameClientApp
{
   public:
    GameClientApp(Graphics::IGraphicsBackend& backend);
    ~GameClientApp();

    void Initialize(Graphics::WindowHandle* handle);
    AppFrameAction Update(float dt, InputSystem& input);
    void Shutdown();

    void OnWindowRecreate(Graphics::WindowHandle*);
    void OnResize(int width, int height);

   private:
    void TransferToScene(uint32_t nextScene);
    PresentationContext PresentCtx();
    PresentationContext PresentCtx(AssetContext);
    AssetContext AssetCtx();

    AssetManager m_assetManager;
    entt::dispatcher m_dispatcher;

    Graphics::IGraphicsBackend& m_backend;
    Graphics::Renderer m_renderer;
    StreamingManager m_streamingManager;
    AssetPool m_assetPool;

    entt::registry m_presentationWorld;

    std::vector<std::unique_ptr<ClientSceneBase>> m_allScenes;
    ClientSceneBase* m_nextScene = nullptr;
    ClientSceneBase* m_currentScene = nullptr;

    FramePerfStatus m_perfStats;
};
}  // namespace OneGame::Engine
