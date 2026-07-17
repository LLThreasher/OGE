#pragma once

#include <vulkan/vulkan.h>

namespace oge::graphics::vulkan
{

struct VulkanFence
{
    VkFence fence = VK_NULL_HANDLE;
};

}  // namespace OneGame::Engine::Graphics::Vulkan
