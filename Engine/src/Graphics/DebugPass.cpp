#include <sstream>
#include "Engine/Graphics/DebugPass.hpp"
#include "stb_easy_font.h"

#define LOGGER_NAME "Engine"
#include "Engine/Logger.hpp"

namespace OneGame::Engine::Graphics
{

    void DebugInfoPass::Initialize(IGraphicsBackend* backend, InitContext& ctx)
    {
        BindingGroupLayoutDesc layout{};
        layout.textureCount = 0;
        layout.bufferCount = 0;
        layout.dynamicBufferMask = 0;
        bindingGroupLayout = backend->CreateBindingGroupLayout(layout);
        {
            GraphicsPipelineDesc desc{};
            ctx.assets.LoadShader("debug.vert.spv", desc.vertexShader);
            ctx.assets.LoadShader("debug.frag.spv", desc.fragmentShader);
            // position
            desc.vertexLayout.push_back(VertexAttributeFormat::Float32x3);
            // color
            desc.vertexLayout.push_back(VertexAttributeFormat::UniformUint8x4);

            desc.bindingGroupLayouts.push_back(bindingGroupLayout);
            desc.writeDepth = false;
            desc.blending = true;
            desc.depthTest = true;
            desc.writeDepth = true;
            desc.depthCompareOp = DepthCompareOp::Less;
            desc.cullMode = CullMode::Back;

            PushConstantRangeDesc cDesc{};
            cDesc.offset = 0;
            cDesc.size = sizeof(math::vec2);
            cDesc.stageFlags = ShaderStage::Vertex;
            desc.pushConstants.push_back(cDesc);

            pipeline = backend->CreateGraphicsPipeline(desc);
        }

        BufferDesc vBuf{};
        vBuf.usage = BufferUsage::Vertex | BufferUsage::TransferDst;
        vBuf.memory = MemoryUsage::GPUOnly;
        vBuf.size = NUM_DEBUG_VERTICES * sizeof(Vertex);
        vertexBuffer = backend->CreateBuffer(vBuf);

        BufferDesc iBuf{};
        iBuf.usage = BufferUsage::Index | BufferUsage::TransferDst;
        iBuf.memory = MemoryUsage::GPUOnly;
        iBuf.size = NUM_DEBUG_INDICES * sizeof(uint16_t);
        indexBuffer = backend->CreateBuffer(iBuf);
    }

    void DebugInfoPass::Shutdown(IGraphicsBackend* backend)
    {
    }

    void DebugInfoPass::Prepare(entt::registry* world)
    {
        std::stringstream ss;
        auto view = world->view<ComponentDebugText>();
        for (auto& entity : view)
        {
            ss << world->get<ComponentDebugText>(entity).text << std::endl;
        }

        std::string s = ss.str();
        const char* cs = s.c_str();
        numQuads = stb_easy_font_print(0.f, 0.f, const_cast<char*>(cs), NULL, vertices, NUM_DEBUG_VERTICES * sizeof(Vertex));
        uint16_t* iptr = indices;
        for (size_t i = 0; i < numQuads; i++)
        {
            vertices[i * 4 + 0].pos *= 3;
            vertices[i * 4 + 1].pos *= 3;
            vertices[i * 4 + 2].pos *= 3;
            vertices[i * 4 + 3].pos *= 3;

            *(iptr++) = i * 4;
            *(iptr++) = i * 4 + 1;
            *(iptr++) = i * 4 + 2;
            *(iptr++) = i * 4 + 2;
            *(iptr++) = i * 4 + 3;
            *(iptr++) = i * 4;
        }
    }

    void DebugInfoPass::Draw(DrawContext& context)
    {
        if (numQuads == 0)
            return;

        auto tCmd = context.transferCmd;
        auto cmd = context.drawCmd;
        tCmd->UpdateBuffer(vertexBuffer, 0, numQuads * 4 * sizeof(Vertex), vertices);
        tCmd->UpdateBuffer(indexBuffer, 0, numQuads * 6 * sizeof(uint16_t), indices);
        tCmd->BufferBarrier(vertexBuffer, BufferUsage::Vertex | BufferUsage::TransferDst, BufferUsage::Vertex);
        tCmd->BufferBarrier(indexBuffer, BufferUsage::Index | BufferUsage::TransferDst, BufferUsage::Index);
        auto extend = context.backend->SwapchainExtend();
        extend.x = 2 / extend.x;
        extend.y = 2 / extend.y;
        
        cmd->BindGraphicsPipeline(pipeline);
        cmd->PushConstants(ShaderStage::Vertex, &extend, sizeof(math::vec2));
        cmd->BindVertexBuffer(vertexBuffer);
        cmd->BindIndexBuffer(indexBuffer, 0, IndexFormat::Uint16);
        cmd->DrawIndexed(numQuads * 6, 1, 0, 0, 0);
    }

	void TestPass::Initialize(IGraphicsBackend* backend, InitContext& ctx)
	{
        BindingGroupLayoutDesc layout{};
        layout.textureCount = 1;
        layout.bufferCount = 1;
        layout.dynamicBufferMask = 1;
        bindingGroupLayout = backend->CreateBindingGroupLayout(layout);

        {
            GraphicsPipelineDesc desc{};
            ctx.assets.LoadShader("test_cube_textured.vert.spv", desc.vertexShader);
            ctx.assets.LoadShader("test_cube_textured.frag.spv", desc.fragmentShader);
            // position
            desc.vertexLayout.push_back(VertexAttributeFormat::Float32x3);
            // color
            desc.vertexLayout.push_back(VertexAttributeFormat::Float32x3);
            // uv
            desc.vertexLayout.push_back(VertexAttributeFormat::Float32x2);

            desc.bindingGroupLayouts.push_back(bindingGroupLayout);
            desc.writeDepth = false;
            desc.blending = true;
            desc.depthTest = true;
            desc.writeDepth = true;
            desc.depthCompareOp = DepthCompareOp::Less;
            desc.cullMode = CullMode::Back;
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

        {
            ctx.assets.LoadTexture("blocks.png", ctx.ringStagingBuffer, backend, texture);

            BindingGroupDesc desc{};
            desc.layout = bindingGroupLayout;
            desc.buffers.push_back(BindingGroupBufferDesc{ ctx.uniformArena.GetBuffer(), sizeof(UBO) });
            desc.textures.push_back(texture);
            bindingGroup = backend->CreateBindingGroup(desc);
        }
	}

	void TestPass::Shutdown(IGraphicsBackend* backend)
	{
	}

	void TestPass::Prepare(entt::registry* world)
	{
	}

	void TestPass::Draw(DrawContext& context)
	{
        m_Time += context.deltaTime;
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
            context.backend->SwapchainAspect(),
            0.1f,
            10.0f);

        auto tCmd = context.transferCmd;
        auto cmd = context.drawCmd;
        if (isFirstFrame)
        {
            tCmd->UpdateBuffer(vertexBuffer, 0, vertices.size() * sizeof(Vertex), vertices.data());
            tCmd->UpdateBuffer(indexBuffer, 0, indices.size() * sizeof(uint32_t), indices.data());
            tCmd->BufferBarrier(vertexBuffer, BufferUsage::Vertex | BufferUsage::TransferDst, BufferUsage::Vertex);
            tCmd->BufferBarrier(indexBuffer, BufferUsage::Index | BufferUsage::TransferDst, BufferUsage::Index);
            isFirstFrame = false;
        }

        auto uniformMemory = context.uniformArena->Allocate(sizeof(UBO));
        std::memcpy(uniformMemory.cpuPtr, &ubo, sizeof(UBO));

        uint32_t offset[1] = { uniformMemory.offset };

        cmd->BindGraphicsPipeline(pipeline);
        cmd->BindVertexBuffer(vertexBuffer);
        cmd->BindIndexBuffer(indexBuffer);
        cmd->BindBindingGroup(bindingGroup, 0, offset);
        cmd->DrawIndexed(indices.size(), 1, 0, 0, 0);
	}
}
