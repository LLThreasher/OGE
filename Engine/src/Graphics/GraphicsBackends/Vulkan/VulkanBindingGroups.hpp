#pragma once

#include <vulkan/vulkan.h>

namespace OneGame::Engine::Graphics::Vulkan
{

    struct VulkanBindingGroupLayout
    {
        VkDescriptorSetLayout layout = VK_NULL_HANDLE;
        uint32_t dynamicBufferMask;
    };

    struct VulkanBindingGroup
    {
        VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
        GPUBindingGroupLayoutHandle layoutHandle;
    };

}