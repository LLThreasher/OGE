#include "fence.hpp"

#include "vulkan.hpp"

namespace oge::graphics::vulkan
{
GPUFenceHandle VulkanBackend::CreateFence(bool signaled) { return GPUFenceHandle{}; }

void VulkanBackend::WaitForFence(GPUFenceHandle) {}

bool VulkanBackend::IsFenceSignaled(GPUFenceHandle) { return false; }

void VulkanBackend::ResetFence(GPUFenceHandle) {}
}  // namespace OneGame::Engine::Graphics::Vulkan