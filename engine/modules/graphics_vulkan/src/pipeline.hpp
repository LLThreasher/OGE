#pragma once

#include <vulkan/vulkan.h>

namespace oge::graphics::vulkan
{

struct VulkanPipeline
{
    VkPipeline pipeline = VK_NULL_HANDLE;
    VkPipelineLayout layout = VK_NULL_HANDLE;

    VkPipelineBindPoint bindingPoint;
};

}  // namespace OneGame::Engine::Graphics::Vulkan
