#pragma once

#include <vulkan/vulkan.h>
#include "Engine/ObjectType.hpp"

namespace OneGame::Engine::Graphics::Vulkan
{

    struct VulkanBindingGroupLayout
    {
        VkDescriptorSetLayout layout = VK_NULL_HANDLE;
        uint32_t dynamicBufferMask;
        uint32_t storageBufferMask;
    };

    struct VulkanBindingGroup
    {
        VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
        GPUBindingGroupLayoutHandle layoutHandle;
    };

}