#include <cmath>
#include "UniformArena.hpp"
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

        auto cmd = backend->CreateCommandList(QueueType::Present);

        cmd->Begin();

        ClearValues values{};
        values.colorClears[0] = { r, g, b, 1.0f };
        cmd->BeginRenderPass(backend->GetCurrentRenderPass(), backend->GetCurrentFrameBuffer(), values);
        cmd->EndRenderPass();

        cmd->End();

        backend->Submit(std::move(cmd));
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

        auto cmd = backend->CreateCommandList(QueueType::Present);

        cmd->Begin();

        ClearValues values{};
        values.colorClears[0] = { r, g, b, 1.0f };
        cmd->BeginRenderPass(backend->GetCurrentRenderPass(), backend->GetCurrentFrameBuffer(), values);
        cmd->BindGraphicsPipeline(pipeline);
        cmd->PushConstants(ShaderStage::Vertex, &angle, sizeof(float));
        cmd->Draw(3, 1, 0, 0);
        cmd->EndRenderPass();
        cmd->End();

        backend->Submit(std::move(cmd));
    }


    void TestRendererCubeWithMVP::Initialize(IGraphicsBackend* backend)
    {
        BindingGroupLayoutDesc layout{};
        layout.bufferCount = 1;
        layout.dynamicBufferMask = 1;
        bindingGroupLayout = backend->CreateBindingGroupLayout(layout);

        {
            GraphicsPipelineDesc desc{};
            LoadShaderBinary("test_cube.vert.spv", desc.vertexShader);
            LoadShaderBinary("test_cube.frag.spv", desc.fragmentShader);
            // position
            desc.vertexLayout.push_back(VertexAttributeFormat::Float32x3);
            // color
            desc.vertexLayout.push_back(VertexAttributeFormat::Float32x3);
            desc.bindingGroupLayouts.push_back(bindingGroupLayout);
            desc.depthTest = true;
            desc.writeDepth = true;
            desc.depthCompareOp = DepthCompareOp::Less;
            pipeline = backend->CreateGraphicsPipeline(desc);
        }

        BufferDesc vBuf{};
        vBuf.usage = BufferUsage::Vertex | BufferUsage::TransferDst;
        vBuf.memory = MemoryUsage::GPUOnly;
        vBuf.size = vertices.size() * sizeof(Vertex);
        vertexBuffer = backend->CreateBuffer(vBuf);

        BufferDesc iBuf{};
        iBuf.usage = BufferUsage::Index | BufferUsage::TransferDst;
        iBuf.memory = MemoryUsage::GPUOnly;
        iBuf.size = indices.size() * sizeof(uint32_t);
        indexBuffer = backend->CreateBuffer(iBuf);

        //uniformBufferPerFrame.resize(backend->FramesInFlight());
        //bindingGroupPerFrame.resize(backend->FramesInFlight());
        //for (size_t i = 0; i < backend->FramesInFlight(); ++i)
        //{
        //    BufferDesc uBuf{};
        //    uBuf.usage = BufferUsage::Uniform | BufferUsage::TransferDst;
        //    uBuf.memory = MemoryUsage::GPUOnly;
        //    uBuf.size = sizeof(UBO);
        //    uniformBufferPerFrame[i] = backend->CreateBuffer(uBuf);

        //    BindingGroupDesc desc{};
        //    desc.layout = bindingGroupLayout;
        //    desc.buffers.push_back(uniformBufferPerFrame[i]);
        //    bindingGroupPerFrame[i] = backend->CreateBindingGroup(desc);
        //}
        uniformArena.Initialize(backend, sizeof(UBO));

        {
              BindingGroupDesc desc{};
              desc.layout = bindingGroupLayout;
              desc.buffers.push_back(BindingGroupBufferDesc { uniformArena.GetGPUBuffer(), sizeof(UBO) });
              bindingGroup = backend->CreateBindingGroup(desc);
        }
    }

    void TestRendererCubeWithMVP::Render(IGraphicsBackend* backend, float dt)
    {
        auto frame = backend->CurrentFrameIndex();
        m_Time += dt;
        UBO ubo{};
        ubo.model = math::rotate(math::mat4(1.0f),
            m_Time * math::radians(90.0f),
            { 0,1,0 });

        ubo.view = math::lookAt(
            math::vec3(2, 2, 2),
            math::vec3(0, 0, 0),
            math::vec3(0, 1, 0));

        ubo.proj = math::perspective(
            math::radians(45.0f),
            backend->SwapchainAspect(),
            0.1f,
            10.0f);

        auto cmd = backend->CreateCommandList(QueueType::Present);
        cmd->Begin();
        if (isFirstFrame)
        {
            cmd->UpdateBuffer(vertexBuffer, 0, vertices.size() * sizeof(Vertex), vertices.data());
            cmd->UpdateBuffer(indexBuffer, 0, indices.size() * sizeof(uint32_t), indices.data());
            cmd->BufferBarrier(vertexBuffer, BufferUsage::Vertex | BufferUsage::TransferDst, BufferUsage::Vertex);
            cmd->BufferBarrier(indexBuffer, BufferUsage::Index | BufferUsage::TransferDst, BufferUsage::Index);
            isFirstFrame = false;
        }

        auto uniformMemory = uniformArena.Allocate(sizeof(UBO));
        std::memcpy(uniformMemory.cpuPtr, &ubo, sizeof(UBO));

        uint32_t offset[1] = { uniformMemory.offset };

        ClearValues values{};
        values.colorClears[0] = { 0.1f, 0.2f, 0.4f, 1.0f };
        values.depthClear = 1.0f;
        values.stencilClear = 0.f;
        cmd->BeginRenderPass(backend->GetCurrentRenderPass(), backend->GetCurrentFrameBuffer(), values);
        cmd->BindGraphicsPipeline(pipeline);
        cmd->BindVertexBuffer(vertexBuffer);
        cmd->BindIndexBuffer(indexBuffer);
        cmd->BindBindingGroup(bindingGroup, 0, offset);
        cmd->DrawIndexed(indices.size(), 1, 0, 0, 0);
        cmd->EndRenderPass();
        cmd->End();

        backend->Submit(std::move(cmd));

        uniformArena.AdvanceFrame();
    }
}
