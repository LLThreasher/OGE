#pragma once

#include <vulkan/vulkan.h>

namespace OneGame::Engine::Graphics::Vulkan
{

struct VulkanFence
{
    VkFence fence = VK_NULL_HANDLE;
};

}  // namespace OneGame::Engine::Graphics::Vulkan
