#pragma once

#include <vulkan/vulkan.h>

#include "oge/graphics/backend.hpp"

struct VmaAllocation_T;
using VmaAllocation = VmaAllocation_T*;

namespace oge::graphics::vulkan
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
