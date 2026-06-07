#pragma once

#include <vulkan/vulkan.h>

namespace OneGame::Engine::Graphics::Vulkan
{

    struct VulkanPipeline
    {
        VkPipeline          pipeline = VK_NULL_HANDLE;
        VkPipelineLayout    layout = VK_NULL_HANDLE;

        VkPipelineBindPoint bindingPoint;
    };

}
