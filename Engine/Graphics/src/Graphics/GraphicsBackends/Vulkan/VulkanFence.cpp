#include "VulkanFence.hpp"

#include "Vulkan.hpp"

namespace OneGame::Engine::Graphics::Vulkan
{
GPUFenceHandle VulkanBackend::CreateFence(bool signaled) { return GPUFenceHandle{}; }

void VulkanBackend::WaitForFence(GPUFenceHandle) {}

bool VulkanBackend::IsFenceSignaled(GPUFenceHandle) { return false; }

void VulkanBackend::ResetFence(GPUFenceHandle) {}
}  // namespace OneGame::Engine::Graphics::Vulkan