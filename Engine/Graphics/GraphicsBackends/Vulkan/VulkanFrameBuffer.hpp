#pragma once

#include <vulkan/vulkan.h>

namespace OneGame::Engine::Graphics::Vulkan
{
    struct VulkanRenderPass
    {
        VkRenderPass handle = VK_NULL_HANDLE;

        RenderPassDesc desc; // store for validation / reuse
    };

    struct VulkanFrameBuffer
    {
        VkFramebuffer handle = VK_NULL_HANDLE;
        VkRenderPass  renderPass = VK_NULL_HANDLE;

        uint32_t width = 0;
        uint32_t height = 0;
    };

}
