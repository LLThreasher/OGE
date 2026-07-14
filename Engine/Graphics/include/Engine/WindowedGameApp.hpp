#pragma once

#include "Engine/AssetManager.hpp"
#include "Engine/GameAppState.hpp"
#include "Engine/GraphicState.hpp"
#include "Engine/Graphics/Renderer.hpp"
#include "Engine/IScene.hpp"
#include "Engine/StreamingManager.hpp"
#include "Engine/ECS/SubsystemRegistry.hpp"
#include "Engine/ECS/RendererRegistry.hpp"

namespace OneGame::Engine
{

class ClientSceneBase;

namespace Graphics
{
class IGraphicsBackend;
class WindowHandle;
}  // namespace Graphics

using WindowedScene = Scene<PresentationContext, const FrameInputData, FrameOutputData>;
using WindowedSceneRunner = SceneRunner<PresentationContext, const FrameInputData, FrameOutputData>;

class GameGraphicApp : public WindowedSceneRunner, ECS::SubsystemRegistryProvider, ECS::RendererRegistryProvider
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

    entt::meta_any m_sceneArgs;
    FramePerfStatus m_perfStats = {};

    uint8_t m_frameIdx = 0;
    std::vector<SceneAction> m_outSceneActions;
};
}  // namespace OneGame::Engine