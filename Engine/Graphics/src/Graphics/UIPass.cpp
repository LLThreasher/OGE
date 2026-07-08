#include "Engine/Graphics/UIPass.hpp"

#include <ft2build.h>
#include FT_FREETYPE_H

#include "RendererInternals.hpp"
#include "Engine/GameAppState.hpp"
#include "Engine/GraphicState.hpp"
#include "Engine/StreamingManager.hpp"

namespace OneGame::Engine::Graphics
{
constexpr uint32_t VERT_COUNT = 512;
constexpr uint32_t INDEX_COUNT = VERT_COUNT / 4 * 6;

void UIPass::Enable(IGraphicsBackend& backend, InitContext& ctxt)
{
    BindingGroupLayoutDesc layout{};
    layout.textureCount = 16;
    layout.bufferCount = 0;
    layout.dynamicBufferMask = 0;
    bindingGroupLayout = backend.CreateBindingGroupLayout(layout);

    {
        GraphicsPipelineDesc desc{};
        if (!ctxt.assets.LoadBlob("ui.vert.spv", desc.vertexShader))
            throw std::runtime_error("failed to load vertex shader");
        if (!ctxt.assets.LoadBlob("ui.frag.spv", desc.fragmentShader))
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

    BufferDesc vBuf{};
    vBuf.usage = BufferUsage::Vertex | BufferUsage::TransferDst;
    vBuf.memory = MemoryUsage::GPUOnly;
    vBuf.size = VERT_COUNT * sizeof(Vertex);
    vertexBuffer = backend.CreateBuffer(vBuf);

    BufferDesc iBuf{};
    iBuf.usage = BufferUsage::Index | BufferUsage::TransferDst;
    iBuf.memory = MemoryUsage::GPUOnly;
    iBuf.size = INDEX_COUNT * sizeof(uint16_t);
    indexBuffer = backend.CreateBuffer(iBuf);

    indices.resize(INDEX_COUNT);
    uint16_t* iptr = indices.data();
    for (size_t i = 0; i < INDEX_COUNT / 6; i++)
    {
        *(iptr++) = i * 4;
        *(iptr++) = i * 4 + 1;
        *(iptr++) = i * 4 + 2;
        *(iptr++) = i * 4 + 2;
        *(iptr++) = i * 4 + 3;
        *(iptr++) = i * 4;
    }
    ctxt.assets.streamingManager.UploadBuffer<UploadType::Immediate>(indices, {.usage=BufferUsage::Index, .buffer=indexBuffer});
}

void UIPass::Disable(IGraphicsBackend& backend)
{
}

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

void UIPass::Draw(DrawContext& ctx)
{
    classedVertices.clear();
    for (auto quad : ctx.world.Get<CmdDrawSprite>())
    {
        auto tl = quad.rect.pos.clampToZero();
        auto br = tl + quad.rect.extent;
        auto uvtl = quad.sprite.uv.pos;
        auto uvbr = quad.sprite.uv.pos + quad.sprite.uv.extent;
        auto& vertices = classedVertices[quad.sprite.texture];
        vertices.push_back(Vertex{{tl.x, tl.y}, {uvtl.x, uvtl.y}, COLOR_WHITE});
        vertices.push_back(Vertex{{br.x, tl.y}, {uvbr.x, uvtl.y}, COLOR_WHITE});
        vertices.push_back(Vertex{{br.x, br.y}, {uvbr.x, uvbr.y}, COLOR_WHITE});
        vertices.push_back(Vertex{{tl.x, br.y}, {uvtl.x, uvbr.y}, COLOR_WHITE});
    }

    if (classedVertices.empty()) return;
    if (ctx.backend.SwapchainRecreated())
    {
        auto extent = ctx.backend.SwapchainExtent();
        math::get_screen_affine(ctx.backend.SwapchainPretransform(), extent.x, extent.y, pushConstant.transform,
                                pushConstant.offset);
    }
    
    auto& tCmd = ctx.transferCmd;
    auto& cmd = ctx.drawCmd;

    uint32_t vBuffOffset = 0;
    for (auto [tex, vertices] : classedVertices)
    {
        tCmd.UpdateBuffer(vertexBuffer, vBuffOffset, vertices.size() * sizeof(Vertex), vertices.data());
        tCmd.BufferBarrier(vertexBuffer, BufferUsage::Vertex | BufferUsage::TransferDst, BufferUsage::Vertex);

        cmd.BindGraphicsPipeline(pipelineHandle);
        cmd.BindBindingGroup(GetOrCreateBindingGroup(ctx.backend, tex), 0);
        cmd.PushConstants(ShaderStage::Vertex, &pushConstant, sizeof(PushConstant));
        cmd.BindVertexBuffer(vertexBuffer, vBuffOffset);
        cmd.BindIndexBuffer(indexBuffer, 0, IndexFormat::Uint16);
        cmd.DrawIndexed(vertices.size() / 4 * 6, 1, 0, 0, 0);

        vBuffOffset += vertices.size() * sizeof(Vertex);
    }
}

}  // namespace OneGame::Engine::Graphics

/*

AssetManager
    - Request()
    - RequestAsync()
    - PollCompleted()
    - Release()

Renderer
    - OnAssetLoaded()

*/