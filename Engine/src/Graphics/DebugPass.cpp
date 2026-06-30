#include "Engine/Graphics/DebugPass.hpp"

#include <sstream>

#include "Engine/GameAppState.hpp"
#include "Engine/Graphics/IGraphicsBackend.hpp"
#include "Engine/Graphics/PresentationObjects.hpp"
#include "RendererInternals.hpp"
#include "stb_easy_font.h"

#define LOGGER_NAME "Engine"
#include "Engine/Logger.hpp"

namespace OneGame::Engine::Graphics
{

void DebugInfoPass::Enable(IGraphicsBackend& backend, InitContext& ctx)
{
    LOG_DEBUG("begin initialize debug info pass");
    BindingGroupLayoutDesc layout{};
    layout.textureCount = 0;
    layout.bufferCount = 0;
    layout.dynamicBufferMask = 0;
    bindingGroupLayout = backend.CreateBindingGroupLayout(layout);
    LOG_DEBUG("binding group layout created");
    {
        GraphicsPipelineDesc desc{};
        if (!ctx.assets.LoadBlob("debug.vert.spv", desc.vertexShader))
            throw std::runtime_error("failed to load vertex shader");
        if (!ctx.assets.LoadBlob("debug.frag.spv", desc.fragmentShader))
            throw std::runtime_error("failed to load fragment shader");
        // position
        desc.vertexLayout.push_back(VertexAttributeFormat::Float32x3);
        // color
        desc.vertexLayout.push_back(VertexAttributeFormat::UniformUint8x4);

        desc.bindingGroupLayouts.push_back(bindingGroupLayout);
        desc.writeDepth = false;
        desc.blending = true;
        desc.cullMode = CullMode::Back;

        PushConstantRangeDesc cDesc{};
        cDesc.offset = 0;
        cDesc.size = sizeof(PushConstant);
        cDesc.stageFlags = ShaderStage::Vertex;
        desc.pushConstants.push_back(cDesc);

        pipeline = backend.CreateGraphicsPipeline(desc);
        LOG_DEBUG("debug pipeline created");
    }

    BufferDesc vBuf{};
    vBuf.usage = BufferUsage::Vertex | BufferUsage::TransferDst;
    vBuf.memory = MemoryUsage::GPUOnly;
    vBuf.size = NUM_DEBUG_VERTICES * sizeof(Vertex);
    vertexBuffer = backend.CreateBuffer(vBuf);
    LOG_DEBUG("debug vbuff created");

    BufferDesc iBuf{};
    iBuf.usage = BufferUsage::Index | BufferUsage::TransferDst;
    iBuf.memory = MemoryUsage::GPUOnly;
    iBuf.size = NUM_DEBUG_INDICES * sizeof(uint16_t);
    indexBuffer = backend.CreateBuffer(iBuf);
    LOG_DEBUG("debug ibuff created");
}

void DebugInfoPass::Disable(IGraphicsBackend& backend) {}

void DebugInfoPass::Draw(DrawContext& ctx)
{
    std::stringstream ss;
    auto view = ctx.world.view<PDebugText>();
    for (auto [_, dbgText] : view.each())
    {
        ss << dbgText.text << std::endl;
    }
    std::string s = ss.str();
    const char* cs = s.c_str();
    // draw background
    if (s.size() > 0)
    {
        auto width = stb_easy_font_width(const_cast<char*>(cs));
        auto height = stb_easy_font_height(const_cast<char*>(cs));

        vertices[0] = {{5.f, 55.f, 0.f}, {0, 0, 0, 128}};
        vertices[1] = {{15.f + width, 55.f, 0.f}, {0, 0, 0, 128}};
        vertices[2] = {{15.f + width, 65.f + height, 0.01f}, {0, 0, 0, 128}};
        vertices[3] = {{5.f, 65.f + height, 0.f}, {0, 0, 0, 128}};

        numQuads = 1;
    }
    else
    {
        numQuads = 0;
        return;
    }

    numQuads += stb_easy_font_print(10.f, 60.f, const_cast<char*>(cs), NULL, &vertices[4],
                                    (NUM_DEBUG_VERTICES - 4) * sizeof(Vertex));

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

    if (ctx.backend.SwapchainRecreated())
    {
        auto extent = ctx.backend.SwapchainExtend();
        math::get_screen_affine(ctx.backend.SwapchainPretransform(), extent.x, extent.y, pushConstant.transform,
                                pushConstant.offset);
    }
    if (numQuads == 0) return;

    auto& tCmd = ctx.transferCmd;
    auto& cmd = ctx.drawCmd;
    tCmd.UpdateBuffer(vertexBuffer, 0, numQuads * 4 * sizeof(Vertex), vertices);
    tCmd.UpdateBuffer(indexBuffer, 0, numQuads * 6 * sizeof(uint16_t), indices);
    tCmd.BufferBarrier(vertexBuffer, BufferUsage::Vertex | BufferUsage::TransferDst, BufferUsage::Vertex);
    tCmd.BufferBarrier(indexBuffer, BufferUsage::Index | BufferUsage::TransferDst, BufferUsage::Index);

    cmd.BindGraphicsPipeline(pipeline);
    cmd.PushConstants(ShaderStage::Vertex, &pushConstant, sizeof(PushConstant));
    cmd.BindVertexBuffer(vertexBuffer);
    cmd.BindIndexBuffer(indexBuffer, 0, IndexFormat::Uint16);
    cmd.DrawIndexed(numQuads * 6, 1, 0, 0, 0);
}

struct TestPassVertex
{
    math::vec3 pos;
    math::vec3 color;
    math::vec2 uv;
};

const std::vector<TestPassVertex> test_vertices = {
    // FRONT (+Z)
    {{-1, -1, 1}, {1, 1, 1}, {0, 0}},
    {{1, -1, 1}, {1, 1, 1}, {1, 0}},
    {{1, 1, 1}, {1, 1, 1}, {1, 1}},
    {{-1, 1, 1}, {1, 1, 1}, {0, 1}},

    // BACK (-Z)
    {{1, -1, -1}, {1, 1, 1}, {0, 0}},
    {{-1, -1, -1}, {1, 1, 1}, {1, 0}},
    {{-1, 1, -1}, {1, 1, 1}, {1, 1}},
    {{1, 1, -1}, {1, 1, 1}, {0, 1}},

    // LEFT (-X)
    {{-1, -1, -1}, {1, 1, 1}, {0, 0}},
    {{-1, -1, 1}, {1, 1, 1}, {1, 0}},
    {{-1, 1, 1}, {1, 1, 1}, {1, 1}},
    {{-1, 1, -1}, {1, 1, 1}, {0, 1}},

    // RIGHT (+X)
    {{1, -1, 1}, {1, 1, 1}, {0, 0}},
    {{1, -1, -1}, {1, 1, 1}, {1, 0}},
    {{1, 1, -1}, {1, 1, 1}, {1, 1}},
    {{1, 1, 1}, {1, 1, 1}, {0, 1}},

    // TOP (+Y)
    {{-1, 1, 1}, {1, 1, 1}, {0, 0}},
    {{1, 1, 1}, {1, 1, 1}, {1, 0}},
    {{1, 1, -1}, {1, 1, 1}, {1, 1}},
    {{-1, 1, -1}, {1, 1, 1}, {0, 1}},

    // BOTTOM (-Y)
    {{-1, -1, -1}, {1, 1, 1}, {0, 0}},
    {{1, -1, -1}, {1, 1, 1}, {1, 0}},
    {{1, -1, 1}, {1, 1, 1}, {1, 1}},
    {{-1, -1, 1}, {1, 1, 1}, {0, 1}},
};

const std::vector<uint32_t> test_indices = {
    0,  1,  2,  2,  3,  0,   // front
    4,  5,  6,  6,  7,  4,   // back
    8,  9,  10, 10, 11, 8,   // left
    12, 13, 14, 14, 15, 12,  // right
    16, 17, 18, 18, 19, 16,  // top
    20, 21, 22, 22, 23, 20   // bottom
};

void TestPass::Enable(IGraphicsBackend& backend, InitContext& ctx)
{
    BindingGroupLayoutDesc layout{};
    layout.textureCount = 1;
    layout.bufferCount = 1;
    layout.dynamicBufferMask = 1;
    bindingGroupLayout = backend.CreateBindingGroupLayout(layout);

    {
        GraphicsPipelineDesc desc{};
        ctx.assets.LoadBlob("test_cube_textured.vert.spv", desc.vertexShader);
        ctx.assets.LoadBlob("test_cube_textured.frag.spv", desc.fragmentShader);
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
        pipeline = backend.CreateGraphicsPipeline(desc);
    }

    testCubeMesh = ctx.assets.LoadMesh(test_vertices, test_indices);

    texture = ctx.assets.LoadTexture("blocks.png");
    {
        BindingGroupDesc desc{};
        desc.layout = bindingGroupLayout;
        desc.buffers.push_back(BindingGroupBufferDesc{ctx.uniformArena.GetBuffer(), sizeof(UBO)});
        desc.textures.push_back(texture);
        bindingGroup = backend.CreateBindingGroup(desc);
    }
}

void TestPass::Disable(IGraphicsBackend& backend) {}

void TestPass::Draw(DrawContext& context)
{
    if (context.currentView != entt::null) return;
    m_Time += context.deltaTime;
    UBO ubo{};
    ubo.model =
        math::rotate(math::translate(math::mat4(1.0f), math::vec3(-2, 0, 2)), m_Time * math::radians(90.0f), {0, 1, 0});

    ubo.view = math::lookAt(math::vec3(5, 5, 5), math::vec3(0, 0, 0), math::vec3(0, 1, 0));

    ubo.proj = math::get_perspective_rot(context.backend.SwapchainPretransform()) *
               math::perspective(math::radians(45.0f), context.backend.SwapchainAspect(), 0.1f, 10.0f);

    auto& tCmd = context.transferCmd;
    auto& cmd = context.drawCmd;

    auto uniformMemory = context.uniformArena.Allocate(sizeof(UBO));
    std::memcpy(uniformMemory.cpuPtr, &ubo, sizeof(UBO));

    uint32_t offset[1] = {uniformMemory.offset};

    cmd.BindGraphicsPipeline(pipeline);
    cmd.BindVertexBuffer(testCubeMesh.vertexBuffer);
    cmd.BindIndexBuffer(testCubeMesh.indexBuffer, 0, IndexFormat::Uint32);
    cmd.BindBindingGroup(bindingGroup, 0, offset);
    cmd.DrawIndexed(testCubeMesh.indexCount, 1, 0, 0, 0);
}
}  // namespace OneGame::Engine::Graphics
