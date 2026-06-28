#pragma once

#include <vulkan/vulkan.h>

#include "Engine/Graphics/IGraphicsBackend.hpp"

namespace OneGame::Engine::Graphics::Vulkan
{
class VulkanBackend;

class VulkanCommandBuffer final : public ICommandList
{
   public:
    explicit VulkanCommandBuffer(QueueType queueType, VkDevice device, VkCommandBuffer cmd, VulkanBackend* backend)
        : m_queueType(queueType), m_device(device), m_cmd(cmd), m_backend(backend)
    {
    }
    virtual ~VulkanCommandBuffer() = default;

    void Begin() override;
    void End() override;

    // ----- Render pass -----
    void BeginRenderPass(const GPURenderPassHandle renderPass, const GPUFrameBufferHandle frameBuffer,
                         const ClearValues& clearValues) override;
    void EndRenderPass() override;

    // ----- Binding -----
    void BindGraphicsPipeline(GPUPipelineHandle) override;
    void BindComputePipeline(GPUPipelineHandle) override;

    void BindVertexBuffer(GPUBufferHandle, uint64_t offset = 0) override;
    void BindIndexBuffer(GPUBufferHandle, uint64_t offset = 0, IndexFormat indexFormat = IndexFormat::Uint32) override;

    void BindBindingGroup(GPUBindingGroupHandle, uint32_t setIndex,
                          std::span<const uint32_t> dynamicOffsets = {}) override;

    void PushConstants(ShaderStage stage, const void* data, uint32_t size) override;

    // ----- Buffer updates -----
    void UpdateBuffer(GPUBufferHandle, uint64_t offset, uint64_t size, const void* data) override;

    virtual void CopyBuffer(GPUBufferHandle src, GPUBufferHandle dst, uint64_t size, uint64_t srcOffset = 0,
                            uint64_t dstOffset = 0) override;

    virtual void CopyBufferToTexture(GPUBufferHandle src, GPUTextureHandle dst, uint32_t bufferOffset) override;

    // ----- Drawing -----
    void Draw(uint32_t vertexCount, uint32_t instanceCount = 1, uint32_t firstVertex = 0,
              uint32_t firstInstance = 0) override;

    void DrawIndexed(uint32_t indexCount, uint32_t instanceCount = 1, uint32_t firstIndex = 0, int32_t vertexOffset = 0,
                     uint32_t firstInstance = 0) override;

    void DrawIndirect(GPUBufferHandle indirectBuffer, uint64_t offset, uint32_t drawCount, uint32_t stride) override;

    void DrawIndexedIndirect(GPUBufferHandle indirectBuffer, uint64_t offset, uint32_t drawCount,
                             uint32_t stride) override;

    // ----- Compute -----
    void Dispatch(uint32_t groupX, uint32_t groupY, uint32_t groupZ) override;

    void DispatchIndirect(GPUBufferHandle indirectBuffer, uint64_t offset) override;

    // ----- Barriers -----
    void BufferBarrier(GPUBufferHandle, BufferUsage before, BufferUsage after) override;

    void TextureBarrier(GPUTextureHandle, TextureState) override;

    VkCommandBuffer GetVulkanCommandBuffer() const { return m_cmd; }
    QueueType GetQueueType() const { return m_queueType; }

    void Clear()
    {
        m_currentPipelineLayout = VK_NULL_HANDLE;
        m_currentBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    }

   private:
    QueueType m_queueType;

    VkDevice m_device = VK_NULL_HANDLE;
    VkCommandBuffer m_cmd = VK_NULL_HANDLE;

    VulkanBackend* m_backend = nullptr;

    // Cached state
    VkPipelineLayout m_currentPipelineLayout = VK_NULL_HANDLE;
    VkPipelineBindPoint m_currentBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
};

}  // namespace OneGame::Engine::Graphics::Vulkan
