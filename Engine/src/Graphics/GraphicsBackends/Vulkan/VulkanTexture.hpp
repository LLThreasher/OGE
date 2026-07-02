#pragma once

#include <vulkan/vulkan.h>

#include "Engine/Graphics/IGraphicsBackend.hpp"

struct VmaAllocation_T;
using VmaAllocation = VmaAllocation_T*;

namespace OneGame::Engine::Graphics::Vulkan
{

struct VulkanTexture
{
    VkImage image = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
    VkSampler sampler = VK_NULL_HANDLE;
    VmaAllocation allocation = nullptr;

    std::vector<TextureState> currentStatePerLayer;

    VkFormat format = VK_FORMAT_UNDEFINED;
    VkImageAspectFlags aspect = VK_IMAGE_ASPECT_NONE;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t mipLevels = 1;
};

}  // namespace OneGame::Engine::Graphics::Vulkan
