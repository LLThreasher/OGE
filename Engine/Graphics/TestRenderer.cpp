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

        float r = float((sin(m_Time) + 1.0) * 0.5);
        float g = float((sin(m_Time + 2.0) + 1.0) * 0.5);
        float b = float((sin(m_Time + 4.0) + 1.0) * 0.5);

        auto* cmd = backend->CreateCommandList(QueueType::Present);

        cmd->Begin();

        ClearValues values{};
        values.colorClears[0] = { r, g, b, 1.0f };
        cmd->BeginRenderPass(backend->GetCurrentRenderPass(), backend->GetCurrentFrameBuffer(), values);

        cmd->EndRenderPass();
        cmd->End();

        backend->Submit(cmd);
    }

    void TestRendererRotateTriangle::Initialize(IGraphicsBackend* backend)
    {
        GraphicsPipelineDesc desc{};
        LoadShaderBinary("test_triangle.vert.spv", desc.vertexShader);
        LoadShaderBinary("test_triangle.frag.spv", desc.fragmentShader);

        PushConstantRangeDesc pc{};
        pc.offset = 0;
        pc.size = sizeof(float);
        pc.stageFlags = ShaderStage::Vertex;
        desc.pushConstants.push_back(pc);

        pipeline = backend->CreateGraphicsPipeline(desc);
    }

    void TestRendererRotateTriangle::Render(IGraphicsBackend* backend, float dt)
    {
        m_Time += dt;

        float rotationSpeed = 0.5f; // radians per second
        angle += rotationSpeed * dt;

        float r = float((sin(m_Time) + 1.0) * 0.5);
        float g = float((sin(m_Time + 2.0) + 1.0) * 0.5);
        float b = float((sin(m_Time + 4.0) + 1.0) * 0.5);

        auto* cmd = backend->CreateCommandList(QueueType::Present);

        cmd->Begin();

        ClearValues values{};
        values.colorClears[0] = { r, g, b, 1.0f };
        cmd->BeginRenderPass(backend->GetCurrentRenderPass(), backend->GetCurrentFrameBuffer(), values);
        cmd->BindGraphicsPipeline(pipeline);
        cmd->PushConstants(ShaderStage::Vertex, &angle, sizeof(float));
        cmd->Draw(3, 1, 0, 0);
        cmd->EndRenderPass();
        cmd->End();

        backend->Submit(cmd);
    }
}
