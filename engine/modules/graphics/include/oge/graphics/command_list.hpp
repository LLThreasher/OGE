#pragma once

#include <cstdint>
#include "oge/graphics/objects.hpp"
#include "oge/graphics/configs.hpp"

namespace oge::graphics {

class ICommandList
{
   public:
    virtual ~ICommandList() = default;

    virtual void SetViewRect(int32_t x, int32_t y, uint32_t extentX, uint32_t extentY) = 0;

    // ----- Render pass -----
    virtual void BeginRenderPass(const GPURenderPassHandle renderPass, const GPUFrameBufferHandle frameBuffer,
                                 const ClearValues& clearValues) = 0;
    virtual void EndRenderPass() = 0;

    // ----- Binding -----
    virtual void BindGraphicsPipeline(GPUPipelineHandle) = 0;
    virtual void BindComputePipeline(GPUPipelineHandle) = 0;

    virtual void BindVertexBuffer(GPUBufferHandle, uint64_t offset = 0) = 0;
    virtual void BindIndexBuffer(GPUBufferHandle, uint64_t offset = 0,
                                 IndexFormat indexFormat = IndexFormat::Uint32) = 0;

    virtual void BindBindingGroup(GPUBindingGroupHandle, uint32_t setIndex,
                                  std::span<const uint32_t> dynamicOffsets = {}) = 0;

    virtual void PushConstants(ShaderStage stage, const void* data, uint32_t size) = 0;

    // ----- Buffer updates -----
    virtual void UpdateBuffer(GPUBufferHandle, uint64_t offset, uint64_t size, const void* data) = 0;

    virtual void CopyBuffer(GPUBufferHandle src, GPUBufferHandle dst, uint64_t size, uint64_t srcOffset = 0,
                            uint64_t dstOffset = 0) = 0;

    virtual void CopyBufferToTexture(GPUBufferHandle src, GPUTextureHandle dst, uint32_t width, uint32_t height,
                                     uint32_t bufferOffset = 0, CopyTextureTarget target = {}) = 0;

    // ----- Drawing -----
    virtual void Draw(uint32_t vertexCount, uint32_t instanceCount = 1, uint32_t firstVertex = 0,
                      uint32_t firstInstance = 0) = 0;

    virtual void DrawIndexed(uint32_t indexCount, uint32_t instanceCount = 1, uint32_t firstIndex = 0,
                             int32_t vertexOffset = 0, uint32_t firstInstance = 0) = 0;

    virtual void DrawIndirect(GPUBufferHandle indirectBuffer, uint64_t offset, uint32_t drawCount, uint32_t stride) = 0;

    virtual void DrawIndexedIndirect(GPUBufferHandle indirectBuffer, uint64_t offset, uint32_t drawCount,
                                     uint32_t stride) = 0;

    // ----- Compute -----
    virtual void Dispatch(uint32_t groupX, uint32_t groupY, uint32_t groupZ) = 0;

    virtual void DispatchIndirect(GPUBufferHandle indirectBuffer, uint64_t offset) = 0;

    // ----- Barriers -----
    virtual void BufferBarrier(GPUBufferHandle, BufferUsage before, BufferUsage after, uint64_t offset = 0) = 0;
    virtual void BufferBarrier(GPUBufferHandle, BufferUsage before, BufferUsage after, uint64_t offset, uint64_t size) = 0;

    virtual void TextureBarrier(GPUTextureHandle, TextureState, uint32_t baseLayer = 0, uint32_t layerCount = 1) = 0;
};
}