#pragma once

#include <vulkan/vulkan.h>

namespace OneGame::Engine::Graphics::Vulkan
{
    struct RenderTextureDesc
    {
        VkFormat format;
        VkAttachmentLoadOp loadOp;
        VkAttachmentStoreOp storeOp;
    };

    struct VulkanRenderPassDesc
    {
        int colorCount;
        bool hasDepth;
        bool isSwapchain;
        std::array<RenderTextureDesc, MaxColorAttachments + 1> renderTextures;
    };

    struct VulkanRenderPass
    {
        VkRenderPass handle = VK_NULL_HANDLE;

        VulkanRenderPassDesc desc = {};
    };

    struct VulkanFrameBuffer
    {
        VkFramebuffer handle = VK_NULL_HANDLE;
        VkRenderPass  renderPass = VK_NULL_HANDLE;

        uint32_t width = 0;
        uint32_t height = 0;
    };

}
