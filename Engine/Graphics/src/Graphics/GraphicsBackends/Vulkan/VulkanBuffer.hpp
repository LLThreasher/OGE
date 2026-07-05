#pragma once

#include <vulkan/vulkan.h>

#include "Vulkan.hpp"

struct VmaAllocation_T;
using VmaAllocation = VmaAllocation_T*;

namespace OneGame::Engine::Graphics::Vulkan
{

struct VulkanBuffer
{
    VkBuffer buffer = VK_NULL_HANDLE;
    VmaAllocation allocation = nullptr;

    VkDeviceSize size = 0;
};

}  // namespace OneGame::Engine::Graphics::Vulkan
