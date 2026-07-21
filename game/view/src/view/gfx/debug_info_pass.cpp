#include "game/view/gfx/debug_info_pass.hpp"

#include <cstddef>
#include <cstdint>
#include <sstream>

#include "internals.hpp"
#include "oge/log.hpp"
#include "stb_easy_font.h"
#include "oge/fmt.hpp"

namespace game::view::gfx
{
void DebugInfoPass::onAttach(InitDrawContext& ctx)
{
    auto& backend = ctx.assets.backend;
    LOG_DEBUG("begin initialize debug info pass");
    BindingGroupLayoutDesc layout{};
    layout.textureCount = 0;
    layout.bufferCount = 0;
    layout.dynamicBufferMask = 0;
    bindingGroupLayout = backend.CreateBindingGroupLayout(layout);
    LOG_DEBUG("binding group layout created");
    {
        GraphicsPipelineDesc desc{};
        if (!ctx.assets.LoadBlob("debug.vert.opt.spv", desc.vertexShader))
            throw std::runtime_error("failed to load vertex shader");
        if (!ctx.assets.LoadBlob("debug.frag.opt.spv", desc.fragmentShader))
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

    vertexArena.Initialize(backend, NUM_DEBUG_VERTICES * sizeof(Vertex));
    LOG_DEBUG("debug vbuff created");

    indexArena.Initialize(backend, NUM_DEBUG_INDICES * sizeof(uint16_t));
    LOG_DEBUG("debug ibuff created");

    for (size_t j = 0; j < backend.FramesInFlight(); ++j)
    {
        auto iAlloc = indexArena.Allocate(NUM_DEBUG_INDICES * sizeof(uint16_t));
        uint16_t* iptr = (uint16_t*)iAlloc.cpuPtr;
        for (size_t i = 0; i < NUM_DEBUG_INDICES / 6; ++i)
        {
            *(iptr++) = i * 4;
            *(iptr++) = i * 4 + 1;
            *(iptr++) = i * 4 + 2;
            *(iptr++) = i * 4 + 2;
            *(iptr++) = i * 4 + 3;
            *(iptr++) = i * 4;
        }
        indexArena.AdvanceFrame();
    }
    indexArena.Flush(backend);
}

void DebugInfoPass::onDetach(InitDrawContext& ctx) {}

void DebugInfoPass::onUpdate(DrawContext& ctx, View view)
{
    if (ctx.backend.SwapchainRecreated())
    {
        auto extent = ctx.backend.SwapchainExtent();
        math::get_screen_affine(ctx.backend.SwapchainPretransform(), extent.x, extent.y, pushConstant.transform,
                                pushConstant.offset);
        LOG_INFO("debug pass swapchain recreate {}", extent);
    }

    std::stringstream ss;
    auto texts = view.Get<CmdDrawDebugText>();
    for (auto& dbgText : texts)
    {
        ss << dbgText.text << std::endl;
    }
    std::string s = ss.str();
    const char* cs = s.c_str();
    auto iAlloc = indexArena.Allocate(NUM_DEBUG_INDICES * sizeof(uint16_t));
    auto vAlloc = vertexArena.Allocate(NUM_DEBUG_VERTICES * sizeof(Vertex));

    vertexArena.AdvanceFrame();
    indexArena.AdvanceFrame();

    auto vertices = (Vertex*)vAlloc.cpuPtr;
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
        numQuads += stb_easy_font_print(10.f, 60.f, const_cast<char*>(cs), NULL, &vertices[4],
                                        (NUM_DEBUG_VERTICES - 4) * sizeof(Vertex));

        for (size_t i = 0; i < numQuads; i++)
        {
            vertices[i * 4 + 0].pos *= 3;
            vertices[i * 4 + 1].pos *= 3;
            vertices[i * 4 + 2].pos *= 3;
            vertices[i * 4 + 3].pos *= 3;
        }
    }
    else
    {
        numQuads = 0;
    }

    for (auto rect : view.Get<CmdDrawDebugRect>())
    {
        static float m = 2.f;
        auto& r = rect.rect;
        auto& color = rect.color;
        size_t i = numQuads * 4;

        vertices[i + 0] = {{r.pos.x, r.pos.y, 0.f}, color};
        vertices[i + 1] = {{r.pos.x + r.extent.x, r.pos.y, 0.f}, color};
        vertices[i + 2] = {{r.pos.x + r.extent.x, r.pos.y + m, 0.f}, color};
        vertices[i + 3] = {{r.pos.x, r.pos.y + m, 0.f}, color};

        vertices[i + 4] = {{r.pos.x + r.extent.x - m, r.pos.y, 0.f}, color};
        vertices[i + 5] = {{r.pos.x + r.extent.x, r.pos.y, 0.f}, color};
        vertices[i + 6] = {{r.pos.x + r.extent.x, r.pos.y + r.extent.y, 0.f}, color};
        vertices[i + 7] = {{r.pos.x + r.extent.x - m, r.pos.y + r.extent.y, 0.f}, color};

        vertices[i + 8] = {{r.pos.x, r.pos.y + r.extent.y - m, 0.f}, color};
        vertices[i + 9] = {{r.pos.x + r.extent.x, r.pos.y + r.extent.y - m, 0.f}, color};
        vertices[i + 10] = {{r.pos.x + r.extent.x, r.pos.y + r.extent.y, 0.f}, color};
        vertices[i + 11] = {{r.pos.x, r.pos.y + r.extent.y, 0.f}, color};

        vertices[i + 12] = {{r.pos.x, r.pos.y, 0.f}, color};
        vertices[i + 13] = {{r.pos.x + m, r.pos.y, 0.f}, color};
        vertices[i + 14] = {{r.pos.x + m, r.pos.y + r.extent.y, 0.f}, color};
        vertices[i + 15] = {{r.pos.x, r.pos.y + r.extent.y, 0.f}, color};

        numQuads += 4;
    }
    if (numQuads == 0) return;

    vertexArena.Flush(ctx.backend);

    auto& cmd = ctx.drawCmd;
    cmd.BindGraphicsPipeline(pipeline);
    cmd.PushConstants(ShaderStage::Vertex, &pushConstant, sizeof(PushConstant));
    cmd.BindVertexBuffer(vertexArena.GetBuffer(), vAlloc.offset);
    cmd.BindIndexBuffer(indexArena.GetBuffer(), iAlloc.offset, IndexFormat::Uint16);
    cmd.DrawIndexed(numQuads * 6, 1, 0, 0, 0);
}
}  // namespace oge::runtime::renderer
