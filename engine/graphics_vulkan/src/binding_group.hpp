#pragma once

#include <vulkan/vulkan.h>

#include "oge/graphics/objects.hpp"

namespace oge::graphics::vulkan
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

}  // namespace OneGame::Engine::Graphics::Vulkan