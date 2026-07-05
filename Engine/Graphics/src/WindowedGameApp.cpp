#include "Engine/WindowedGameApp.hpp"

#include "Engine/Platform/Stopwatch.hpp"
#include "Engine/GraphicState.hpp"
#include "Engine/Graphics/DebugPass.hpp"
#include "Engine/Graphics/IGraphicsBackend.hpp"

#define LOGGER_NAME "Engine"
#include "Engine/Logger.hpp"

#define TEST_TERRAIN_0

namespace OneGame::Engine
{
using namespace Graphics;

constexpr uint32_t DEBUG_SCENE3 = 0;

inline PresentationContext GameGraphicApp::PresentCtx()
{
    return {m_assetManager, m_streamingManager, m_backend, m_assetPool, m_dispatcher, m_sceneArgs, m_renderer};
}

inline PresentationContext GameGraphicApp::PresentCtx(AssetContext assets)
{
    return {assets, m_dispatcher, m_sceneArgs, m_renderer};
}

inline AssetContext GameGraphicApp::AssetCtx() { return {m_assetManager, m_streamingManager, m_backend, m_assetPool}; }

GameGraphicApp::GameGraphicApp(IGraphicsBackend& backend) : m_backend(backend)
{
}

GameGraphicApp::~GameGraphicApp() = default;

void GameGraphicApp::Initialize(WindowHandle* handle)
{
    m_backend.Initialize(BackendDesc{handle, FrameTimePreference::VSync});
    LOG_DEBUG("Backend created");
    m_streamingManager.Initialize(m_backend);
    LOG_DEBUG("StreamingManager created");
    AssetContext assets = AssetCtx();
    m_renderer.Initialize(assets);
    LOG_DEBUG("Renderer created");

    PresentationContext ctx = PresentCtx(assets);
    WindowedSceneRunner::Initialize(ctx);
}

void GameGraphicApp::Shutdown()
{
    m_backend.WaitDeviceIdle();
    auto assets = AssetCtx();
    m_renderer.Shutdown(assets);
    m_streamingManager.Shutdown(m_backend);
    m_backend.Shutdown();
}

AppFrameAction GameGraphicApp::Update(float dt, InputSystem& input)
{
    FramePerfStatus perfStats{};
    auto watch = stopwatch::start();
    AppFrameAction appRes = AppFrameAction::None;

    m_dispatcher.update();

    PresentationContext pctx = PresentCtx();

    std::vector<SceneAction> outSceneActions;
    FrameInputData frame{
        dt,
        m_perfStats,
        input,
    };
    FrameOutputData frameOut{
        dt,
        m_perfStats,
        m_graphicsSubmissionQueue,
        outSceneActions,
    };
    m_graphicsSubmissionQueue.Clear();

    WindowedSceneRunner::Update(pctx, frame, frameOut);

    perfStats.logicTime = watch.restart();

    auto res = m_backend.BeginFrame();
    if (res == BeginFrameAction::RecreateSurface) return appRes | AppFrameAction::WaitSurface;
    if (res != BeginFrameAction::Continue) return appRes | AppFrameAction::None;

    perfStats.inputProcessingTime = watch.restart();

    auto& tcmd = m_backend.CreateCommandList(QueueType::Transfer);
    tcmd.Begin();
    m_streamingManager.RunUploadStep(m_backend, tcmd);
    tcmd.End();

    if (m_backend.SwapchainRecreated())
    {
        auto swapExtend = m_backend.SwapchainExtent();
        auto swapPretransform = m_backend.SwapchainPretransform();
        m_dispatcher.enqueue<SurfaceRecreateEvent>({swapExtend, swapPretransform});
    }
    m_renderer.Render(pctx, m_graphicsSubmissionQueue, dt);

    perfStats.assetUploadTime = watch.restart();

    auto endRes = m_backend.EndFrame();

    perfStats.renderSubmitTime = watch.restart();
    m_perfStats = perfStats;

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

void GameGraphicApp::OnResize(int width, int height) { m_backend.Resize(width, height); }

void GameGraphicApp::OnWindowRecreate(Graphics::WindowHandle* handle) { m_backend.RecreateSurface(handle); }
}  // namespace OneGame::Engine
