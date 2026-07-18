#include "game/client.hpp"

#include "oge/graphics/backend.hpp"
#include "oge/graphics/vulkan/create_backend.hpp"
#include "oge/runtime/gfx/chunk_allocator2.hpp"
#include "oge/runtime/gfx/skyline_allocator.hpp"

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
}

void Client::Initialize(WindowHandle* handle)
{
    auto backend = oge::graphics::vulkan::CreateVulkanBackend();
    m_backend = m_ctx.Emplace<std::unique_ptr<IGraphicsBackend>>(backend.release())->get();
    m_backend->Initialize(BackendDesc{handle, FrameTimePreference::VSync});
}

AppFrameAction Client::Update(float dt, oge::input::RawInputStream& input)
{
    AppFrameAction appRes = AppFrameAction::None;
    auto res = m_backend->BeginFrame();
    if (res == BeginFrameAction::RecreateSurface) return appRes | AppFrameAction::WaitSurface;
    if (res != BeginFrameAction::Continue) return appRes | AppFrameAction::None;

    SceneRunner::Update(dt, input);

    auto endRes = m_backend->EndFrame();
    if (endRes == EndFrameAction::RecreateSurface) return appRes | AppFrameAction::WaitSurface;

    return appRes;
}

void Client::Shutdown() { m_backend->Shutdown(); }

void Client::OnWindowRecreate(WindowHandle*) {}

void Client::OnResize(int width, int height) {}
}  // namespace game
