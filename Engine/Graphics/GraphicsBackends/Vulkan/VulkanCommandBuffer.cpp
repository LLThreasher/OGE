#pragma once

#include "VulkanCommandBuffer.hpp"
#include "VulkanPipeline.hpp"
#include "VulkanFrameBuffer.hpp"
#include "VulkanBuffer.hpp"
#include "VulkanBindingGroups.hpp"
#include "VulkanTexture.hpp"


namespace OneGame::Engine::Graphics::Vulkan
{
    void VulkanCommandBuffer::Begin()
    {
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        vkBeginCommandBuffer(m_cmd, &beginInfo);
    }

    void VulkanCommandBuffer::End()
    {
        vkEndCommandBuffer(m_cmd);
    }
#undef max
    void VulkanCommandBuffer::BeginRenderPass(const GPURenderPassHandle renderPass, const GPUFrameBufferHandle frameBuffer, const ClearValues& clearValues)
    {
        VulkanRenderPass& rp = m_backend->m_renderPasses.Get(renderPass);
        VulkanFrameBuffer& fb = m_backend->m_frameBuffers.Get(frameBuffer);

        VkRenderPassBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        beginInfo.renderPass = rp.handle;
        beginInfo.framebuffer = fb.handle;
        beginInfo.renderArea.offset = { 0, 0 };
        beginInfo.renderArea.extent = { fb.width, fb.height };

        int clearCount = 0;
        std::array<VkClearValue, 5> tmpClearValues;
		for (; clearCount < rp.desc.colorCount; clearCount++)
		{
			std::memcpy(&tmpClearValues[clearCount].color.float32, &clearValues.colorClears[clearCount], sizeof(float) * 4);
		}
		if (rp.desc.hasDepth)
		{
            tmpClearValues[clearCount].depthStencil.depth = clearValues.depthClear;
            tmpClearValues[clearCount].depthStencil.stencil = clearValues.stencilClear;
			clearCount++;
		}

        beginInfo.clearValueCount = clearCount;
        beginInfo.pClearValues = tmpClearValues.data();

        vkCmdBeginRenderPass(m_cmd, &beginInfo, VK_SUBPASS_CONTENTS_INLINE);
    }

    void VulkanCommandBuffer::EndRenderPass()
    {
        vkCmdEndRenderPass(m_cmd);
    }

    void VulkanCommandBuffer::BindGraphicsPipeline(GPUPipelineHandle handle)
    {
        VulkanPipeline& pipeline = m_backend->m_pipelines.Get(handle);

        vkCmdBindPipeline(
            m_cmd,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            pipeline.pipeline);

        m_currentPipelineLayout = pipeline.layout;
        m_currentBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    }

    void VulkanCommandBuffer::BindComputePipeline(GPUPipelineHandle handle)
    {
        VulkanPipeline& pipeline = m_backend->m_pipelines.Get(handle);

        vkCmdBindPipeline(
            m_cmd,
            VK_PIPELINE_BIND_POINT_COMPUTE,
            pipeline.pipeline);

        m_currentPipelineLayout = pipeline.layout;
        m_currentBindPoint = VK_PIPELINE_BIND_POINT_COMPUTE;
    }

    void VulkanCommandBuffer::BindVertexBuffer(
        GPUBufferHandle handle,
        uint64_t offset)
    {
        VulkanBuffer& buffer = m_backend->m_buffers.Get(handle);

        VkBuffer vkBuffer = buffer.buffer;
        VkDeviceSize vkOffset = offset;

        vkCmdBindVertexBuffers(
            m_cmd,
            0,
            1,
            &vkBuffer,
            &vkOffset);
    }

    void VulkanCommandBuffer::BindIndexBuffer(
        GPUBufferHandle handle,
        uint64_t offset)
    {
        VulkanBuffer& buffer = m_backend->m_buffers.Get(handle);

        vkCmdBindIndexBuffer(
            m_cmd,
            buffer.buffer,
            offset,
            VK_INDEX_TYPE_UINT32); // or stored in buffer metadata
    }

    void VulkanCommandBuffer::BindBindingGroup(
        GPUBindingGroupHandle handle,
        uint32_t setIndex,
        std::span<const uint32_t> dynamicOffsets)
    {
        VulkanBindingGroup& group = m_backend->m_bindingGroups.Get(handle);

        vkCmdBindDescriptorSets(
            m_cmd,
            m_currentBindPoint,
            m_currentPipelineLayout,
            setIndex,
            1,
            &group.descriptorSet,
            static_cast<uint32_t>(dynamicOffsets.size()),
            dynamicOffsets.data());
    }

    void VulkanCommandBuffer::PushConstants(
        ShaderStage stage,
        const void* data,
        uint32_t size)
    {
        VkShaderStageFlags flags = ToVkShaderStage(stage);

        vkCmdPushConstants(
            m_cmd,
            m_currentPipelineLayout,
            flags,
            0,
            size,
            data);
    }

    void VulkanCommandBuffer::UpdateBuffer(
        GPUBufferHandle handle,
        uint64_t offset,
        uint64_t size,
        const void* data)
    {
        VulkanBuffer& buffer = m_backend->m_buffers.Get(handle);

        vkCmdUpdateBuffer(
            m_cmd,
            buffer.buffer,
            offset,
            size,
            data);
    }

    void VulkanCommandBuffer::Draw(
        uint32_t vertexCount,
        uint32_t instanceCount,
        uint32_t firstVertex,
        uint32_t firstInstance)
    {
        vkCmdDraw(
            m_cmd,
            vertexCount,
            instanceCount,
            firstVertex,
            firstInstance);
    }

    void VulkanCommandBuffer::DrawIndexed(
        uint32_t indexCount,
        uint32_t instanceCount,
        uint32_t firstIndex,
        int32_t vertexOffset,
        uint32_t firstInstance)
    {
        vkCmdDrawIndexed(
            m_cmd,
            indexCount,
            instanceCount,
            firstIndex,
            vertexOffset,
            firstInstance);
    }

    void VulkanCommandBuffer::DrawIndirect(
        GPUBufferHandle indirectBuffer,
        uint64_t offset,
        uint32_t drawCount,
        uint32_t stride)
    {
        VulkanBuffer& buffer =
            m_backend->m_buffers.Get(indirectBuffer);

        assert(m_currentBindPoint == VK_PIPELINE_BIND_POINT_GRAPHICS);
        assert(offset % 4 == 0);

        vkCmdDrawIndirect(
            m_cmd,
            buffer.buffer,
            offset,
            drawCount,
            stride);
    }

    void VulkanCommandBuffer::DrawIndexedIndirect(
        GPUBufferHandle indirectBuffer,
        uint64_t offset,
        uint32_t drawCount,
        uint32_t stride)
    {
        VulkanBuffer& buffer =
            m_backend->m_buffers.Get(indirectBuffer);

        assert(m_currentBindPoint == VK_PIPELINE_BIND_POINT_GRAPHICS);
        assert(offset % 4 == 0);

        vkCmdDrawIndexedIndirect(
            m_cmd,
            buffer.buffer,
            offset,
            drawCount,
            stride);
    }

    void VulkanCommandBuffer::Dispatch(
        uint32_t groupX,
        uint32_t groupY,
        uint32_t groupZ)
    {
        vkCmdDispatch(m_cmd, groupX, groupY, groupZ);
    }

    void VulkanCommandBuffer::DispatchIndirect(
        GPUBufferHandle handle,
        uint64_t offset)
    {
        VulkanBuffer& buffer =
            m_backend->m_buffers.Get(handle);

        // Ensure compute pipeline bind point is active
        assert(m_currentBindPoint == VK_PIPELINE_BIND_POINT_COMPUTE);

        vkCmdDispatchIndirect(
            m_cmd,
            buffer.buffer,
            offset);
    }

    void VulkanCommandBuffer::BufferBarrier(
        GPUBufferHandle handle,
        BufferUsage before,
        BufferUsage after)
    {
        VulkanBuffer& buffer = m_backend->m_buffers.Get(handle);

        VkBufferMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        barrier.srcAccessMask = ConvertAccessMask(before);
        barrier.dstAccessMask = ConvertAccessMask(after);
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.buffer = buffer.buffer;
        barrier.offset = 0;
        barrier.size = VK_WHOLE_SIZE;

        vkCmdPipelineBarrier(
            m_cmd,
            ConvertPipelineStage(before),
            ConvertPipelineStage(after),
            0,
            0, nullptr,
            1, &barrier,
            0, nullptr);
    }

    void VulkanCommandBuffer::TextureBarrier(
        GPUTextureHandle handle,
        TextureState newState)
    {
        VulkanTexture& texture =
            m_backend->m_textures.Get(handle);

        TextureState oldState = texture.currentState;

        if (oldState == newState)
            return; // no transition needed

        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;

        barrier.srcAccessMask = ToVkAccessMask(oldState);
        barrier.dstAccessMask = ToVkAccessMask(newState);

        barrier.oldLayout = ToVkImageLayout(oldState);
        barrier.newLayout = ToVkImageLayout(newState);

        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

        barrier.image = texture.image;

        barrier.subresourceRange.aspectMask = texture.aspect;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = texture.mipLevels;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;

        vkCmdPipelineBarrier(
            m_cmd,
            ToVkPipelineStage(oldState),
            ToVkPipelineStage(newState),
            0,
            0, nullptr,
            0, nullptr,
            1, &barrier);

        texture.currentState = newState;
    }
}