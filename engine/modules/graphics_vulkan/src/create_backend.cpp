#include "oge/graphics/vulkan/create_backend.hpp"

#include "vulkan.hpp"

namespace oge::graphics::vulkan
{
std::unique_ptr<IGraphicsBackend> CreateVulkanBackend()
{
    return std::make_unique<VulkanBackend>();
}
}  // namespace oge::graphics::vulkan
