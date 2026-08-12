#include "metal_command_buffer.hpp"

#define LOGGER_NAME "Metal"
#include "oge/log.hpp"

namespace oge::graphics::metal
{

MetalCommandBuffer::MetalCommandBuffer(MTL::CommandBuffer* mtlCB,
                                       MetalBackend& backend)
    : m_mtlCmdBuf(mtlCB), m_backend(backend)
{
}

MetalCommandBuffer::~MetalCommandBuffer()
{
    if (m_encoder != nullptr)
    {
        m_encoder->endEncoding();
        m_encoder = nullptr;
    }
}

bool MetalCommandBuffer::beginEncoder()
{
    if (m_encoder != nullptr) return true;

    auto& sc = m_backend.m_swapchain;
    if (sc.currentDrawable == nullptr) return false;

    // Create depth texture on first use.
    if (!sc.currentDepthTexture.IsValid() &&
        sc.extent.x > 0 && sc.extent.y > 0)
    {
        sc.currentDepthTexture = m_backend.CreateDepthTexture(
            sc.extent.x, sc.extent.y);
    }

    auto* rpDesc = MTL::RenderPassDescriptor::alloc()->init();
    auto* ca = rpDesc->colorAttachments()->object(0);
    ca->setTexture(sc.currentDrawable->texture());
    ca->setLoadAction(MTL::LoadActionClear);
    ca->setClearColor(MTL::ClearColor::Make(0.1f, 0.12f, 0.15f, 1.0f));
    ca->setStoreAction(MTL::StoreActionStore);

    // Depth attachment.
    if (sc.currentDepthTexture.IsValid())
    {
        auto* t = m_backend.m_textures.Get(sc.currentDepthTexture);
        if (t != nullptr && t->texture.get() != nullptr)
        {
            auto* da = rpDesc->depthAttachment();
            da->setTexture(t->texture.get());
            da->setLoadAction(MTL::LoadActionClear);
            da->setClearDepth(1.0);
            da->setStoreAction(MTL::StoreActionDontCare);
        }
    }

    m_encoder = m_mtlCmdBuf->renderCommandEncoder(rpDesc);
    rpDesc->release();
    return m_encoder != nullptr;
}

void MetalCommandBuffer::SetViewRect(int32_t x, int32_t y, uint32_t w,
                                     uint32_t h)
{
    if (beginEncoder())
        // Metal's viewport origin is bottom-left (OpenGL convention).
        // Invert Y to match Vulkan's top-left origin so shaders, projection
        // matrices, and texture coordinates all match.
        m_encoder->setViewport(
            MTL::Viewport{(double)x, (double)(y + h), (double)w, -(double)h,
                          0.0, 1.0});
}

void MetalCommandBuffer::BeginRenderPass(const GPURenderPassDesc&)
{
    // Render pass is begun lazily on first draw/bind.
}

void MetalCommandBuffer::EndRenderPass()
{
    if (m_encoder != nullptr)
    {
        m_encoder->endEncoding();
        m_encoder = nullptr;
    }
}

void MetalCommandBuffer::BindGraphicsPipeline(GPUPipelineHandle handle)
{
    if (!handle.IsValid()) return;
    m_currentPipeline = handle;
    if (!beginEncoder()) return;

    auto* p = m_backend.m_pipelines.Get(handle);
    if (p == nullptr || p->renderPipeline.get() == nullptr) return;
    m_encoder->setRenderPipelineState(p->renderPipeline.get());
}

void MetalCommandBuffer::BindComputePipeline(GPUPipelineHandle) {}

void MetalCommandBuffer::BindVertexBuffer(GPUBufferHandle handle,
                                          uint64_t offset)
{
    if (beginEncoder())
    {
        auto* b = m_backend.m_buffers.Get(handle);
        if (b != nullptr && b->buffer.get() != nullptr)
            // Use high slot to avoid clashing with uniform buffers at slot 0.
            m_encoder->setVertexBuffer(b->buffer.get(), offset,
                                       kVertexBufferSlot);
    }
}

void MetalCommandBuffer::BindIndexBuffer(GPUBufferHandle handle,
                                         uint64_t offset, IndexFormat fmt)
{
    auto* b = m_backend.m_buffers.Get(handle);
    if (b != nullptr)
    {
        m_indexBuffer = b->buffer;
        m_indexBufferOffset = offset;
        m_indexType = fmt == IndexFormat::Uint16
                          ? MTL::IndexTypeUInt16
                          : MTL::IndexTypeUInt32;
    }
}

void MetalCommandBuffer::BindBindingGroup(GPUBindingGroupHandle handle,
                                          uint32_t setIndex,
                                          std::span<const uint32_t> dynamicOffsets)
{
    if (!beginEncoder()) return;
    (void)setIndex;

    auto* group = m_backend.m_bindingGroups.Get(handle);
    if (group == nullptr) return;

    // Bind textures to fragment shader slots.
    for (size_t i = 0; i < group->desc.textures.size(); ++i)
    {
        auto* t = m_backend.m_textures.Get(group->desc.textures[i]);
        if (t != nullptr && t->texture.get() != nullptr)
        {
            m_encoder->setFragmentTexture(t->texture.get(), i);
            // Create a default sampler for this texture slot.
            auto* samplerDesc = MTL::SamplerDescriptor::alloc()->init();
            samplerDesc->setMinFilter(MTL::SamplerMinMagFilterLinear);
            samplerDesc->setMagFilter(MTL::SamplerMinMagFilterLinear);
            samplerDesc->setMipFilter(MTL::SamplerMipFilterLinear);
            auto* device = t->texture->device();
            auto* sampler = device->newSamplerState(samplerDesc);
            samplerDesc->release();
            m_encoder->setFragmentSamplerState(sampler, i);
            sampler->release();
        }
    }

    // Bind buffers to vertex/fragment shader slots starting at 0.
    for (size_t i = 0; i < group->desc.buffers.size(); ++i)
    {
        auto& bd = group->desc.buffers[i];
        auto* b = m_backend.m_buffers.Get(bd.gpuBuffer);
        if (b == nullptr || b->buffer.get() == nullptr) continue;

        uint64_t off = (i < dynamicOffsets.size()) ? dynamicOffsets[i] : 0;
        m_encoder->setVertexBuffer(b->buffer.get(), off, i);
        m_encoder->setFragmentBuffer(b->buffer.get(), off, i);
    }
}

void MetalCommandBuffer::PushConstants(ShaderStage stage, const void* data,
                                       uint32_t size)
{
    if (beginEncoder() && data != nullptr && size > 0)
    {
        if (stage == ShaderStage::Vertex || stage == ShaderStage::Fragment)
            m_encoder->setVertexBytes(data, size, 0);
    }
}

void MetalCommandBuffer::UpdateBuffer(GPUBufferHandle, uint64_t, uint64_t,
                                      const void*)
{
}

void MetalCommandBuffer::CopyBuffer(GPUBufferHandle, GPUBufferHandle,
                                    uint64_t, uint64_t, uint64_t)
{
}

void MetalCommandBuffer::CopyBufferToTexture(GPUBufferHandle srcBuf,
                                             GPUTextureHandle dstTex,
                                             uint32_t w, uint32_t h,
                                             uint32_t bufOff,
                                             CopyTextureTarget tgt)
{
    auto* src = m_backend.m_buffers.Get(srcBuf);
    auto* dst = m_backend.m_textures.Get(dstTex);
    if (src == nullptr || dst == nullptr) return;
    if (src->buffer.get() == nullptr || dst->texture.get() == nullptr) return;

    // Bytes per row: width × 4 bytes (RGBA), aligned to 256.
    uint32_t bpr = ((w * 4) + 255) & ~255u;
    auto* blit = m_mtlCmdBuf->blitCommandEncoder();
    blit->copyFromBuffer(
        src->buffer.get(), bufOff,
        bpr,
        bpr * h,
        MTL::Size{w, h, 1},
        dst->texture.get(),
        tgt.baseTextureLayer, tgt.mipLevel,
        MTL::Origin{tgt.offsetX, tgt.offsetY, 0});
    blit->endEncoding();
}

void MetalCommandBuffer::Draw(uint32_t vertexCount, uint32_t instanceCount,
                              uint32_t firstVertex, uint32_t firstInstance)
{
    if (!beginEncoder()) return;
    auto* p = m_backend.m_pipelines.Get(m_currentPipeline);
    auto prim = p != nullptr ? p->primitiveType
                             : MTL::PrimitiveTypeTriangle;
    m_encoder->drawPrimitives(prim, firstVertex, vertexCount, instanceCount,
                              0);  // baseInstance not needed for firstInstance=0
    (void)firstInstance;
}

void MetalCommandBuffer::DrawIndexed(uint32_t indexCount, uint32_t instCount,
                                     uint32_t firstIndex, int32_t vertOff,
                                     uint32_t firstInst)
{
    if (!beginEncoder()) return;
    if (m_indexBuffer.get() == nullptr) return;
    auto* p = m_backend.m_pipelines.Get(m_currentPipeline);
    auto prim = p != nullptr ? p->primitiveType
                             : MTL::PrimitiveTypeTriangle;
    // Metal has no "firstIndex" — offset into the index buffer includes it.
    uint64_t idxOff = m_indexBufferOffset
        + firstIndex * (m_indexType == MTL::IndexTypeUInt16 ? 2u : 4u);
    m_encoder->drawIndexedPrimitives(
        prim, indexCount, m_indexType,
        m_indexBuffer.get(), idxOff,
        instCount, vertOff, firstInst);
}

void MetalCommandBuffer::Dispatch(uint32_t gx, uint32_t gy, uint32_t gz)
{
    if (!beginEncoder()) return;
    auto* p = m_backend.m_pipelines.Get(m_currentPipeline);
    if (p != nullptr && p->computePipeline.get() != nullptr)
    {
        m_encoder->endEncoding();
        m_encoder = nullptr;

        auto* ce = m_mtlCmdBuf->computeCommandEncoder();
        ce->setComputePipelineState(p->computePipeline.get());
        ce->dispatchThreadgroups(
            MTL::Size{gx, gy, gz},
            MTL::Size{1, 1, 1});
        ce->endEncoding();
    }
}

}  // namespace oge::graphics::metal
