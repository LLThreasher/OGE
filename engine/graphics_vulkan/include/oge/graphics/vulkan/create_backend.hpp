#pragma once

#include <memory>
#include "oge/graphics/backend.hpp"

namespace oge::graphics::vulkan
{
std::unique_ptr<IGraphicsBackend> CreateVulkanBackend();
} // namespace oge::graphics::vulkan
