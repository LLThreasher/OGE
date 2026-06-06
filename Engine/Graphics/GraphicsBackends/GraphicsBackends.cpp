#include <fstream>
#include "../../EngineLogger.hpp"
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

	void LoadShaderBinary(const char* filePath, std::vector<char>& output)
	{
		std::ifstream file(filePath, std::ios::ate | std::ios::binary);

		if (!file.is_open())
		{
			LOG_ERROR("Failed to open file {}", filePath);
			return;
		}

		// 2. Get the file size and allocate the vector buffer
		std::streamsize size = file.tellg();
		output.resize(size);

		// 3. Move the file pointer back to the beginning and read the data
		file.seekg(0, std::ios::beg);
		file.read(output.data(), size);
	}
}
