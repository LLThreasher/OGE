#pragma once

#include <vulkan/vulkan.h>

#include "vulkan.hpp"

struct VmaAllocation_T;
using VmaAllocation = VmaAllocation_T*;

namespace oge::graphics::vulkan
{

struct VulkanBuffer
{
    VkBuffer buffer = VK_NULL_HANDLE;
    VmaAllocation allocation = nullptr;

    VkDeviceSize size = 0;
};

}  // namespace OneGame::Engine::Graphics::Vulkan
