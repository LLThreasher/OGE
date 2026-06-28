#include "Engine/GameApp.hpp"

#include "Engine/Graphics/DebugPass.hpp"
#include "Engine/Graphics/IGraphicsBackend.hpp"
#include "Engine/Scenes/DebugScene.hpp"

#define LOGGER_NAME "Engine"
#include "Engine/Logger.hpp"

#define TEST_TERRAIN_0

namespace OneGame::Engine
{
using namespace Graphics;

constexpr uint32_t EMPTY_SCENE = 0;
constexpr uint32_t DEBUG_SCENE = 1;
constexpr uint32_t DEBUG_SCENE2 = 2;

GameClientApp::GameClientApp(IGraphicsBackend& backend)
    : m_backend(backend), m_assetPool(m_assetManager, m_streamingManager, m_backend)
{
    m_allScenes.push_back(std::unique_ptr<ClientSceneBase>(new DebugScene2()));
    TransferToScene(0);
}

GameClientApp::~GameClientApp() = default;

void GameClientApp::Initialize(WindowHandle* handle)
{
    m_backend.Initialize(BackendDesc{handle, FrameTimePreference::VSync});
    LOG_DEBUG("Backend created");
    m_streamingManager.Initialize(m_backend);
    LOG_DEBUG("StreamingManager created");
    m_renderer.Initialize(m_backend, m_assetPool);
    LOG_DEBUG("Renderer created");

    AppContext appCtx{
        m_assetManager,
        m_dispatcher,
    };

    PresentationContext ctx{
        appCtx, m_backend, m_renderer, m_streamingManager, m_assetPool,
    };

    for (auto& scene : m_allScenes)
    {
        scene->Initialize(ctx);
    }
}

void GameClientApp::TransferToScene(uint32_t scene)
{
    assert(scene < m_allScenes.size());
    m_nextScene = m_allScenes[scene].get();
}

void GameClientApp::Shutdown()
{
    m_backend.WaitDeviceIdle();
    m_renderer.Shutdown(m_backend);
    m_streamingManager.Shutdown(m_backend);
    m_backend.Shutdown();
}

AppFrameAction GameClientApp::Update(float dt, InputSystem& input)
{
    AppFrameAction appRes = AppFrameAction::WaitFPS60;
    auto res = m_backend.BeginFrame();
    if (res == BeginFrameAction::RecreateSurface) return appRes | AppFrameAction::WaitSurface;
    if (res != BeginFrameAction::Continue) return appRes | AppFrameAction::None;

    m_dispatcher.update();

    AppContext ctx{
        m_assetManager,
        m_dispatcher,
    };
    PresentationContext pctx{
        ctx, m_backend, m_renderer, m_streamingManager, m_assetPool,
    };

    std::vector<SceneAction> outSceneActions;
    FrameInputData frame{
        dt,
        input,
    };
    FrameOutputData frameOut{
        m_presentationWorld,
        outSceneActions,
    };
    if (m_nextScene != nullptr)
    {
        if (m_currentScene != nullptr)
        {
            m_currentScene->Exit(pctx);
        }
        m_nextScene->Enter(pctx);
        m_currentScene = m_nextScene;
        m_nextScene = nullptr;
    }
    assert(m_currentScene != nullptr);
    m_presentationWorld.clear();
    m_currentScene->Update(pctx, frame, frameOut);

    m_renderer.Prepare(m_backend, m_presentationWorld, dt);
    auto& tcmd = m_backend.CreateCommandList(QueueType::Transfer);
    tcmd.Begin();
    m_streamingManager.RunUploadStep(m_backend, tcmd);
    tcmd.End();

    m_renderer.Render(m_backend, dt);
    auto endRes = m_backend.EndFrame();

    for (auto& action : frameOut.outSceneActions)
    {
        if (action.type == SceneActionType::SetMouseWarpping)
        {
            if (action.setMouseWarpping.enabled)
            {
                appRes = appRes | AppFrameAction::WrapMouse;
            }
            else
            {
                appRes = appRes | AppFrameAction::UnwrapMouse;
            }
        }
    }

    if (endRes == EndFrameAction::RecreateSurface) return appRes | AppFrameAction::WaitSurface;

    return appRes;
}

void GameClientApp::OnResize(int width, int height) { m_backend.Resize(width, height); }

void GameClientApp::OnWindowRecreate(Graphics::WindowHandle* handle)
{
    m_backend.RecreateSurface(handle);
    auto swapExtend = m_backend.SwapchainExtend();
    m_dispatcher.enqueue<SurfaceRecreateEvent>(swapExtend.x, swapExtend.y);
}
}  // namespace OneGame::Engine
