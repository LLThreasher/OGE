#include <cmath>
#include "TestRenderer.hpp"

namespace OneGame::Engine::Graphics
{
    void TestRenderer::Initialize(IGraphicsBackend* backend)
    {
    }

    void TestRenderer::Render(IGraphicsBackend* backend, float dt)
    {
        m_Time += dt;

        float r = (std::sin(m_Time) * 0.5f) + 0.5f;
        float g = (std::cos(m_Time) * 0.5f) + 0.5f;

        auto* cmd = backend->CreateCommandList(QueueType::Present);

        cmd->Begin();

        ClearValues values{};
        values.colorClears[0] = { 0.1f, 0.2f, 0.4f, 1.0f };
        cmd->BeginRenderPass(backend->GetCurrentRenderPass(), backend->GetCurrentFrameBuffer(), values);

        cmd->EndRenderPass();
        cmd->End();

        backend->Submit(cmd);
    }
}
