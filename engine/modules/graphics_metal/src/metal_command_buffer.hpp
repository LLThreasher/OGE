#pragma once

#include <Metal/Metal.hpp>

#include <array>
#include <cstdint>
#include <span>

#include "oge/graphics/command_list.hpp"

namespace oge::graphics::metal
{

class MetalBackend;  // forward — full definition in metal.hpp

/// Implements ICommandList by wrapping MTL::CommandBuffer and
/// MTL::RenderCommandEncoder.  Created once per frame by
/// MetalBackend::CreateCommandList.
class MetalCommandBuffer final : public ICommandList
{
   public:
    // Metal buffer index for vertex data, above uniform/storage slots.
    static constexpr uint32_t kVertexBufferSlot = 30;

    MetalCommandBuffer() : m_mtlCmdBuf(nullptr), m_backend(nullptr) {}
    MetalCommandBuffer(MTL::CommandBuffer* mtlCB, MetalBackend& backend);
    ~MetalCommandBuffer() override;

    /// Reset this wrapper for a new MTL::CommandBuffer (used by the pool).
    void Reset(MTL::CommandBuffer* mtlCB, MetalBackend& backend);

    // ICommandList
    void SetViewRect(int32_t x, int32_t y, uint32_t w, uint32_t h) override;

    void BeginRenderPass(const GPURenderPassDesc& desc) override;
    void EndRenderPass() override;

    void BindGraphicsPipeline(GPUPipelineHandle) override;
    void BindComputePipeline(GPUPipelineHandle) override;

    void BindVertexBuffer(GPUBufferHandle, uint64_t offset = 0) override;
    void BindIndexBuffer(GPUBufferHandle, uint64_t offset = 0,
                         IndexFormat = IndexFormat::Uint32) override;

    void BindBindingGroup(GPUBindingGroupHandle, uint32_t setIndex,
                          std::span<const uint32_t> offs = {}) override;

    void PushConstants(ShaderStage stage, const void* data,
                       uint32_t size) override;

    void UpdateBuffer(GPUBufferHandle, uint64_t offset, uint64_t size,
                      const void* data) override;

    void CopyBuffer(GPUBufferHandle src, GPUBufferHandle dst, uint64_t size,
                    uint64_t srcOff = 0, uint64_t dstOff = 0) override;

    void CopyBufferToTexture(GPUBufferHandle src, GPUTextureHandle dst,
                             uint32_t w, uint32_t h,
                             uint32_t bufOff = 0,
                             CopyTextureTarget tgt = {}) override;

    void Draw(uint32_t vertexCount, uint32_t instanceCount = 1,
              uint32_t firstVertex = 0, uint32_t firstInstance = 0) override;

    void DrawIndexed(uint32_t indexCount, uint32_t instanceCount = 1,
                     uint32_t firstIndex = 0, int32_t vertexOffset = 0,
                     uint32_t firstInstance = 0) override;

    void DrawIndirect(GPUBufferHandle, uint64_t, uint32_t,
                      uint32_t) override {}
    void DrawIndexedIndirect(GPUBufferHandle, uint64_t, uint32_t,
                             uint32_t) override {}

    void Dispatch(uint32_t gx, uint32_t gy, uint32_t gz) override;
    void DispatchIndirect(GPUBufferHandle, uint64_t) override {}

    void BufferBarrier(GPUBufferHandle, BufferUsage, BufferUsage,
                       uint64_t = 0) override {}
    void BufferBarrier(GPUBufferHandle, BufferUsage, BufferUsage, uint64_t,
                       uint64_t) override {}
    void TextureBarrier(GPUTextureHandle, TextureState, uint32_t = 0,
                        uint32_t = 1) override {}

   private:
    MTL::CommandBuffer* m_mtlCmdBuf;
    MetalBackend* m_backend;

    MTL::RenderCommandEncoder* m_encoder = nullptr;
    GPUPipelineHandle m_currentPipeline;
    GPURenderPassDesc m_renderPassDesc = {};
    std::array<float, 4> m_clearColor = {0.1f, 0.12f, 0.15f, 1.0f};
    float m_clearDepth = 1.0f;

    NS::SharedPtr<MTL::Buffer> m_indexBuffer;
    uint64_t m_indexBufferOffset = 0;
    MTL::IndexType m_indexType = MTL::IndexTypeUInt32;

    bool beginEncoder();
};

}  // namespace oge::graphics::metal
