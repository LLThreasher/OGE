#include "game/client.hpp"

#include "game/scene.hpp"
#include "oge/graphics/backend.hpp"
#include "oge/graphics/vulkan/create_backend.hpp"
#include "oge/log.hpp"
#include "oge/platform/window_app.hpp"
#include "oge/runtime/gfx/chunk_allocator2.hpp"
#include "oge/runtime/gfx/skyline_allocator.hpp"
#include "oge/platform/perf.hpp"
#include "oge/runtime/typed_registry.hpp"
#include "oge/stopwatch.hpp"
#include "oge/fmt.hpp"
#include "game/input/input_source.hpp"

#include "game/sim/registry.hpp"
#include "game/view/renderer.hpp"
#include "game/events.hpp"

namespace game
{
Client::Client()
    : m_ctx(m_metaWorld),
      m_am(*m_ctx.Emplace<AssetManager>()),
      m_sm(*m_ctx.Emplace<StreamingManager>()),
      m_ap(*m_ctx.Emplace<AssetPool>()),
      m_ca(*m_ctx.Emplace<DynamicChunkAllocator>()),
      m_sa(*m_ctx.Emplace<DynamicSkylineAllocator>()),
      SceneRunner(m_ctx)
{
    using namespace sim;
    using namespace view;
    using namespace input;
    RegisterInputSources(m_anyFactory);
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

    m_events.enqueue(SurfaceRecreateEvent{m_backend->SwapchainExtent(), m_backend->SwapchainPretransform()});
}

AppFrameAction Client::Update(float dt, InputProvider PollInputs)
{
    FramePerfStatus perfStats{};
    AppFrameAction appRes = AppFrameAction::None;
    auto watch = oge::Stopwatch::Start();
    auto& backend = *m_backend;

    m_input.NewFrame();
    // we block if we are waiting for surface
    PollInputs(m_input, m_waitingSurface);

    if (m_waitingSurface) {
        return appRes | AppFrameAction::WaitSurface;
    }

    perfStats.inputProcessingTime = watch.Restart();

    auto res = backend.BeginFrame();

    if (res == BeginFrameAction::RecreateSurface) {
        m_waitingSurface = true;
        return appRes | AppFrameAction::WaitSurface;
    }
    if (res != BeginFrameAction::Continue) return appRes | AppFrameAction::None;

    watch.Restart();
    PollInputs(m_input, false);
    perfStats.inputProcessingTime += watch.Restart();

    m_events.update();
    SceneRunner::UpdateScene({dt, m_input, m_perfStats, appRes});
    if (appRes != AppFrameAction::None)
    {
        LOG_DEBUG("new appRes: {}", (uint32_t)appRes);
    }

    perfStats.logicTime = watch.Restart();

    auto& tcmd = backend.CreateCommandList(QueueType::Transfer);
    m_sm.RunUploadStep(backend, tcmd);

    perfStats.assetUploadTime = watch.Restart();

    CurrentScene()->Render(dt);

    auto endRes = backend.EndFrame();
    if (endRes == EndFrameAction::RecreateSurface) return appRes | AppFrameAction::WaitSurface;

    perfStats.renderSubmitTime = watch.Restart();
    m_perfStats = perfStats;

    return appRes;
}

void Client::Shutdown()
{
    m_backend->WaitDeviceIdle();
    SceneRunner::DetachScene();
    m_sm.Shutdown(*m_backend);
    m_backend->Shutdown();
}

void Client::OnWindowRecreate(WindowHandle* handle)
{
    m_waitingSurface = false;
    m_backend->RecreateSurface(handle);
    LOG_DEBUG("trigger surface recreate {}", m_backend->SwapchainExtent());
    m_events.enqueue(SurfaceRecreateEvent{m_backend->SwapchainExtent(), m_backend->SwapchainPretransform()});
}

void Client::OnResize(int width, int height)
{
    m_backend->Resize(width, height);
    m_events.enqueue(SurfaceRecreateEvent{m_backend->SwapchainExtent(), m_backend->SwapchainPretransform()});
}
}  // namespace game
