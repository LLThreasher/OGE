#pragma once

#include <cassert>
#include <vulkan/vulkan.h>
#include "vk_mem_alloc.h"

#include "../Vulkan.hpp"
#include "VulkanBuffer.hpp"


namespace OneGame::Engine::Graphics::Vulkan
{
    GPUBufferHandle VulkanBackend::CreateBuffer(
        const BufferDesc& desc)
    {
        VulkanBuffer result{};
        result.size = desc.size;

        // --- Buffer info ---
        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = desc.size;
		if (HasFlag(desc.usage, BufferUsage::Vertex))
			bufferInfo.usage |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
		if (HasFlag(desc.usage, BufferUsage::Index))
			bufferInfo.usage |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
		if (HasFlag(desc.usage, BufferUsage::Uniform))
			bufferInfo.usage |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
		if (HasFlag(desc.usage, BufferUsage::Storage))
			bufferInfo.usage |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
		if (HasFlag(desc.usage, BufferUsage::Indirect))
			bufferInfo.usage |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
		if (HasFlag(desc.usage, BufferUsage::TransferSrc))
			bufferInfo.usage |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
		if (HasFlag(desc.usage, BufferUsage::TransferDst))
			bufferInfo.usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        // --- Allocation info ---
        VmaAllocationCreateInfo allocInfo{};
        switch (desc.memory) {
		case MemoryUsage::GPUOnly:
			allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
            allocInfo.flags = 0;
			break;
		case MemoryUsage::CPUToGPU:
			allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
			allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
			break;
		case MemoryUsage::GPUToCPU:
			allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
			allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
            break;
        }

        // --- Create buffer ---
        VmaAllocationInfo allocationInfo{};

        VK_CHECK_RESULT(vmaCreateBuffer(
            m_device.m_allocator,
            &bufferInfo,
            &allocInfo,
            &result.buffer,
            &result.allocation,
            &allocationInfo));

		if (allocInfo.flags & VMA_ALLOCATION_CREATE_MAPPED_BIT)
		{
			result.mappedData = allocationInfo.pMappedData;
		}

        return m_buffers.Create(result);
    }

    void VulkanBackend::DestroyBuffer(GPUBufferHandle handle)
    {
        auto buffer = m_buffers.Get(handle);
        if (buffer.buffer != VK_NULL_HANDLE)
        {
            vmaDestroyBuffer(
                m_device.m_allocator,
                buffer.buffer,
                buffer.allocation);

            buffer.buffer = VK_NULL_HANDLE;
            buffer.allocation = nullptr;
            buffer.mappedData = nullptr;
            buffer.size = 0;
        }
        m_buffers.Destroy(handle);
    }
}
