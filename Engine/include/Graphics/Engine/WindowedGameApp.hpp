#pragma once

#include "Engine/GameAppState.hpp"
#include "Engine/GraphicState.hpp"
#include "Engine/StreamingManager.hpp"
#include "Engine/AssetManager.hpp"
#include "Engine/Graphics/Renderer.hpp"

namespace OneGame::Engine
{

class ClientSceneBase;

namespace Graphics
{
    class IGraphicsBackend;
    class WindowHandle;
}

class GameGraphicApp
{
   public:
    GameGraphicApp(Graphics::IGraphicsBackend& backend);
    ~GameGraphicApp();

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

    Graphics::SubmissionQueue m_graphicsSubmissionQueue;

    std::vector<std::unique_ptr<ClientSceneBase>> m_allScenes;
    ClientSceneBase* m_nextScene = nullptr;
    ClientSceneBase* m_currentScene = nullptr;

    FramePerfStatus m_perfStats = {};
};
}