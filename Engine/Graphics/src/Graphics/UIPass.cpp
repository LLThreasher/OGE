#include "Engine/Graphics/UIPass.hpp"

#include <ft2build.h>
#include FT_FREETYPE_H

#include "RendererInternals.hpp"
#include "Engine/GameAppState.hpp"
#include "Engine/GraphicState.hpp"

namespace OneGame::Engine::Graphics
{
void UIPass::Enable(IGraphicsBackend& backend, InitContext& ctxt)
{
    BindingGroupLayoutDesc layout{};
    layout.textureCount = 16;
    layout.bufferCount = 0;
    layout.dynamicBufferMask = 0;
    bindingGroupLayout = backend.CreateBindingGroupLayout(layout);

    {
        GraphicsPipelineDesc desc{};
        assert(ctxt.assets.LoadBlob("ui.vert.spv", desc.vertexShader));
        assert(ctxt.assets.LoadBlob("ui.frag.spv", desc.fragmentShader));
        // position
        desc.vertexLayout.push_back(VertexAttributeFormat::Uint16x2);
        // uv
        desc.vertexLayout.push_back(VertexAttributeFormat::UniformUint16x2);
        // color
        desc.vertexLayout.push_back(VertexAttributeFormat::UniformUint8x4);
        // texIndex
        desc.vertexLayout.push_back(VertexAttributeFormat::Float32x2);

        PushConstantRangeDesc cDesc{};
        cDesc.size = sizeof(math::vec2);
        cDesc.offset = 0;
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
    vBuf.size = 1024 * sizeof(Vertex);
    vertexBuffer = backend.CreateBuffer(vBuf);

    BufferDesc iBuf{};
    iBuf.usage = BufferUsage::Index | BufferUsage::TransferDst;
    iBuf.memory = MemoryUsage::GPUOnly;
    iBuf.size = 4 * 256 * sizeof(uint16_t);
    indexBuffer = backend.CreateBuffer(iBuf);

    {
        BindingGroupDesc desc{};
        desc.layout = bindingGroupLayout;
        bindingGroup = backend.CreateBindingGroup(desc);
    }
}

void UIPass::Disable(IGraphicsBackend& backend) {}

void UIPass::Draw(DrawContext& context)
{

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