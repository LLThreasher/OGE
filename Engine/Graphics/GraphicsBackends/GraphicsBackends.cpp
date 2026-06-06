#include "../IGraphicsBackend.hpp"
#include "Vulkan.hpp"

namespace OneGame::Engine::Graphics
{
	std::unique_ptr<IGraphicsBackend> CreateBackend(BackendType type)
	{
		if (type == BackendType::Vulkan)
		{
			return std::make_unique<Vulkan::VulkanBackend>();
		}
		else
		{
			throw std::runtime_error("undefined backend");
		}
	}
}