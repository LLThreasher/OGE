#pragma once

#include <vulkan/vulkan.h>
#include "../Vulkan.hpp"
#include "VulkanBindingGroups.hpp"
#include "VulkanTexture.hpp"
#include "VulkanBuffer.hpp"

namespace OneGame::Engine::Graphics::Vulkan
{
    GPUBindingGroupLayoutHandle
        VulkanBackend::CreateBindingGroupLayout(const BindingGroupLayoutDesc& desc)
    {
        VulkanBindingGroupLayout layout{};
        layout.dynamicBufferMask = desc.dynamicBufferMask;

        std::vector<VkDescriptorSetLayoutBinding> bindings;

        uint32_t bindingIndex = 0;

        //
        // Textures
        //
        for (int i = 0; i < desc.textureCount; ++i)
        {
            VkDescriptorSetLayoutBinding binding{};
            binding.binding = bindingIndex++;
            binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            binding.descriptorCount = 1;
            binding.stageFlags =
                VK_SHADER_STAGE_VERTEX_BIT |
                VK_SHADER_STAGE_FRAGMENT_BIT;

            bindings.push_back(binding);
        }

        //
        // Buffers
        //
        for (uint32_t i = 0; i < desc.bufferCount; ++i)
        {
            bool dynamic = (desc.dynamicBufferMask & (1 << i)) != 0;

            VkDescriptorSetLayoutBinding binding{};
            binding.binding = bindingIndex++;
            binding.descriptorType =
                dynamic ?
                VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC :
                VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;

            binding.descriptorCount = 1;
            binding.stageFlags =
                VK_SHADER_STAGE_VERTEX_BIT |
                VK_SHADER_STAGE_FRAGMENT_BIT;

            bindings.push_back(binding);
        }

        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType =
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount =
            static_cast<uint32_t>(bindings.size());
        layoutInfo.pBindings = bindings.data();

        if (vkCreateDescriptorSetLayout(
            m_device.device,
            &layoutInfo,
            nullptr,
            &layout.layout) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create descriptor set layout");
        }

        return m_bindingGroupLayouts.Create(layout);
    }

    void VulkanBackend::DestroyBindingGroupLayout(
        GPUBindingGroupLayoutHandle handle)
    {
        auto& layout = m_bindingGroupLayouts.Get(handle);

        if (layout.layout != VK_NULL_HANDLE)
        {
            vkDestroyDescriptorSetLayout(
                m_device.device,
                layout.layout,
                nullptr);
        }

        layout = {};
        m_bindingGroupLayouts.Destroy(handle);
    }

    GPUBindingGroupHandle
        VulkanBackend::CreateBindingGroup(const BindingGroupDesc& desc)
    {
        VulkanBindingGroup group{};
        group.layoutHandle = desc.layout;

        auto& layout =
            m_bindingGroupLayouts.Get(desc.layout);

        //
        // 1️⃣ Allocate descriptor set
        //
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType =
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = m_descriptorPool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &layout.layout;

        if (vkAllocateDescriptorSets(
            m_device.device,
            &allocInfo,
            &group.descriptorSet) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to allocate descriptor set");
        }

        //
        // 2️⃣ Write descriptors
        //
        std::vector<VkWriteDescriptorSet> writes;

        uint32_t bindingIndex = 0;

        //
        // Textures
        //
        for (auto texHandle : desc.textures)
        {
            auto& tex = m_textures.Get(texHandle);

            VkDescriptorImageInfo imageInfo{};
            imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            imageInfo.imageView = tex.view;
            imageInfo.sampler = tex.sampler;

            VkWriteDescriptorSet write{};
            write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write.dstSet = group.descriptorSet;
            write.dstBinding = bindingIndex++;
            write.descriptorType =
                VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            write.descriptorCount = 1;
            write.pImageInfo = &imageInfo;

            writes.push_back(write);
        }

        //
        // Buffers
        //
        for (size_t i = 0; i < desc.buffers.size(); ++i)
        {
            auto& bufHandle = desc.buffers[i];
            auto& buf = m_buffers.Get(bufHandle.gpuBuffer);

            VkDescriptorBufferInfo bufferInfo{};
            bufferInfo.buffer = buf.buffer;
            bufferInfo.offset = 0;
            bufferInfo.range = bufHandle.stride == 0 ? VK_WHOLE_SIZE : bufHandle.stride;

            VkWriteDescriptorSet write{};
            write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write.dstSet = group.descriptorSet;
            write.dstBinding = bindingIndex++;
            write.descriptorType =
                (layout.dynamicBufferMask & (1 << i)) != 0 ?
                VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC :
                VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            write.descriptorCount = 1;
            write.pBufferInfo = &bufferInfo;

            writes.push_back(write);
        }

        vkUpdateDescriptorSets(
            m_device.device,
            static_cast<uint32_t>(writes.size()),
            writes.data(),
            0,
            nullptr);

        return m_bindingGroups.Create(group);
    }

    void VulkanBackend::DestroyBindingGroup(
        GPUBindingGroupHandle handle)
    {
        auto& group = m_bindingGroups.Get(handle);

        vkFreeDescriptorSets(
            m_device.device,
            m_descriptorPool,
            1,
            &group.descriptorSet);

        group = {};
        m_bindingGroups.Destroy(handle);
    }
}
