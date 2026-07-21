#include "oge/runtime/gfx/ui_pass.hpp"

#include <cstdint>

#include "oge/graphics/backend.hpp"
#include "oge/graphics/command_list.hpp"
#include "oge/graphics/objects.hpp"
#include "oge/runtime/asset_ctx.hpp"
#include "oge/runtime/asset_manager.hpp"
#include "oge/runtime/gfx/draw_context.hpp"
#include "oge/runtime/streaming_manager.hpp"

using namespace oge::graphics;

namespace oge::runtime::gfx
{
constexpr uint32_t VERT_COUNT = 65536;
constexpr uint32_t INDEX_COUNT = VERT_COUNT / 4 * 6;

GPUBindingGroupHandle UIPass::GetOrCreateBindingGroup(IGraphicsBackend& backend, GPUTextureHandle texture)
{
    assert(texture.IsValid());
    auto it = cachedBindingGroups.find(texture);
    if (it != cachedBindingGroups.end())
    {
        return it->second;
    }
    else
    {
        BindingGroupDesc desc{};
        desc.layout = bindingGroupLayout;
        desc.textures.push_back(texture);
        auto [newIt, inserted] = cachedBindingGroups.emplace(texture, backend.CreateBindingGroup(desc));
        return newIt->second;
    }
}

void UIPass::onAttach(InitDrawContext& ctx)
{
    auto& backend = ctx.assets.backend;
    BindingGroupLayoutDesc layout{};
    layout.textureCount = 16;
    layout.bufferCount = 0;
    layout.dynamicBufferMask = 0;
    bindingGroupLayout = backend.CreateBindingGroupLayout(layout);

    {
        GraphicsPipelineDesc desc{};
        if (!ctx.assets.LoadBlob("ui.vert.opt.spv", desc.vertexShader))
            throw std::runtime_error("failed to load vertex shader");
        if (!ctx.assets.LoadBlob("ui.frag.opt.spv", desc.fragmentShader))
            throw std::runtime_error("failed to load fragment shader");
        // position
        desc.vertexLayout.push_back(VertexAttributeFormat::Uint16x2);
        // uv
        desc.vertexLayout.push_back(VertexAttributeFormat::UniformUint16x2);
        // color
        desc.vertexLayout.push_back(VertexAttributeFormat::UniformUint8x4);

        PushConstantRangeDesc cDesc{};
        cDesc.offset = 0;
        cDesc.size = sizeof(PushConstant);
        cDesc.stageFlags = ShaderStage::Vertex;
        desc.pushConstants.push_back(cDesc);

        desc.bindingGroupLayouts.push_back(bindingGroupLayout);
        desc.writeDepth = false;
        desc.blending = true;
        desc.depthTest = false;
        desc.depthCompareOp = DepthCompareOp::Never;
        desc.cullMode = CullMode::Back;
        pipelineHandle = backend.CreateGraphicsPipeline(desc);
    }

    vertexArena.Initialize(backend, VERT_COUNT * sizeof(Vertex));
    indexArena.Initialize(backend, INDEX_COUNT * sizeof(uint16_t));

    for (size_t j = 0; j < backend.FramesInFlight(); ++j)
    {
        auto iAlloc = indexArena.Allocate(INDEX_COUNT * sizeof(uint16_t));
        uint16_t* iptr = (uint16_t*)iAlloc.cpuPtr;
        for (size_t i = 0; i < INDEX_COUNT / 6; ++i)
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

void UIPass::onDetach(InitDrawContext& ctx) {}

void UIPass::onUpdate(DrawContext& ctx, View view)
{
    if (ctx.backend.SwapchainRecreated())
    {
        auto extent = ctx.backend.SwapchainExtent();
        math::get_screen_affine(ctx.backend.SwapchainPretransform(), extent.x, extent.y, pushConstant.transform,
                                pushConstant.offset);
    }

    classedVertices.clear();
    for (auto quad : view.Get<CmdDrawSprite>())
    {
        auto tl = quad.rect.pos.clampToZero();
        auto br = tl + quad.rect.extent;
        auto uvtl = quad.sprite.uv.pos;
        auto uvbr = quad.sprite.uv.pos + quad.sprite.uv.extent;
        auto& vertices = classedVertices[quad.sprite.texture];
        vertices.push_back(Vertex{{tl.x, tl.y}, {uvtl.x, uvtl.y}, quad.color});
        vertices.push_back(Vertex{{br.x, tl.y}, {uvbr.x, uvtl.y}, quad.color});
        vertices.push_back(Vertex{{br.x, br.y}, {uvbr.x, uvbr.y}, quad.color});
        vertices.push_back(Vertex{{tl.x, br.y}, {uvtl.x, uvbr.y}, quad.color});
    }

    if (classedVertices.empty()) return;

    auto& cmd = ctx.drawCmd;

    cmd.BindGraphicsPipeline(pipelineHandle);
    auto vBufBase = vertexArena.Allocate(0).offset;
    cmd.BindVertexBuffer(vertexArena.GetBuffer(), vBufBase);
    cmd.BindIndexBuffer(indexArena.GetBuffer(), indexArena.Allocate(INDEX_COUNT).offset, IndexFormat::Uint16);
    uint32_t bindingSetIdx = 0;
    for (const auto& it : classedVertices)
    {
        // tCmd.UpdateBuffer(vertexBuffer, vBuffOffset, vertices.size() * sizeof(Vertex), vertices.data());
        // tCmd.BufferBarrier(vertexBuffer, BufferUsage::Vertex | BufferUsage::TransferDst, BufferUsage::Vertex);

        auto& vertices = it.second;
        auto& tex = it.first;
        auto vBuf = vertexArena.Allocate(vertices.size() * sizeof(Vertex));
        memcpy(vBuf.cpuPtr, vertices.data(), vertices.size() * sizeof(Vertex));

        cmd.BindBindingGroup(GetOrCreateBindingGroup(ctx.backend, tex), 0);
        cmd.PushConstants(ShaderStage::Vertex, &pushConstant, sizeof(PushConstant));
        cmd.DrawIndexed(vertices.size() / 4 * 6, 1, 0, (vBuf.offset - vBufBase) / sizeof(Vertex), 0);
    }
    vertexArena.Flush(ctx.backend);
    vertexArena.AdvanceFrame();
    indexArena.AdvanceFrame();
}
}  // namespace oge::runtime::gfx
