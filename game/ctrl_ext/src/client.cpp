#include "game/client.hpp"

#include "oge/graphics/backend.hpp"
#include "oge/graphics/vulkan/create_backend.hpp"
#include "oge/runtime/gfx/chunk_allocator2.hpp"
#include "oge/runtime/gfx/skyline_allocator.hpp"
#include "oge/platform/perf.hpp"
#include "oge/stopwatch.hpp"

#include "game/sim/subsystem.hpp"
#include "game/view/renderer.hpp"

namespace game
{
Client::Client()
    : m_ctx(m_metaWorld),
      SceneRunner(m_ctx),
      m_am(*m_ctx.Emplace<AssetManager>()),
      m_sm(*m_ctx.Emplace<StreamingManager>()),
      m_ap(*m_ctx.Emplace<AssetPool>()),
      m_ca(*m_ctx.Emplace<DynamicChunkAllocator>()),
      m_sa(*m_ctx.Emplace<DynamicSkylineAllocator>())
{
    using namespace sim;
    using namespace view;
    RegisterSubsystems(m_anyFactory);
    RegisterRenderers(m_anyFactory);
}

void Client::Initialize(WindowHandle* handle)
{
    auto backend_ptr = oge::graphics::vulkan::CreateVulkanBackend();
    m_backend = m_ctx.Emplace<std::unique_ptr<IGraphicsBackend>>(backend_ptr.release())->get();
    auto& backend = *m_backend;
    backend.Initialize(BackendDesc{handle, FrameTimePreference::VSync});
    m_sm.Initialize(backend);
}

AppFrameAction Client::Update(float dt, oge::input::RawInputStream& input)
{
    FramePerfStatus perfStats{};
    perfStats.cpuUsage = GetCPUUsage();
    auto watch = oge::Stopwatch::Start();
    auto& backend = *m_backend;

    AppFrameAction appRes = AppFrameAction::None;
    auto res = backend.BeginFrame();
    if (res == BeginFrameAction::RecreateSurface) return appRes | AppFrameAction::WaitSurface;
    if (res != BeginFrameAction::Continue) return appRes | AppFrameAction::None;

    perfStats.inputProcessingTime = watch.Restart();

    auto& tcmd = backend.CreateCommandList(QueueType::Transfer);
    tcmd.Begin();
    m_sm.RunUploadStep(backend, tcmd);
    tcmd.End();

    perfStats.assetUploadTime = watch.Restart();

    SceneRunner::Update({dt, input, m_perfStats});

    perfStats.logicTime = watch.Restart();

    auto endRes = backend.EndFrame();
    if (endRes == EndFrameAction::RecreateSurface) return appRes | AppFrameAction::WaitSurface;

    perfStats.renderSubmitTime = watch.Restart();
    m_perfStats = perfStats;

    return appRes;
}

void Client::Shutdown()
{
    SceneRunner::DetachCurrentScene();

    m_sm.Shutdown(*m_backend);
    m_backend->Shutdown();
}

void Client::OnWindowRecreate(WindowHandle* handle)
{
    m_backend->RecreateSurface(handle);
}

void Client::OnResize(int width, int height)
{
    m_backend->Resize(width, height);
}
}  // namespace game
