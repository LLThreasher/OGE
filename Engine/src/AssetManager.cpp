#include "Engine/AssetManager.hpp"
#include "stb_image.h"


namespace OneGame::Engine
{
	bool TryLoadPNG(std::vector<char> data, int& width, int& height, void* result)
	{
		int texChannels;
		if (result == nullptr)
		{
			if (!stbi_info_from_memory((unsigned char*)(data.data()), data.size(), &width, &height, &texChannels))
			{
				//LOG_ERROR("Failed to read image info! {}", filePath);
				return false;
			}
			return true;
		}

		stbi_uc* pixels = stbi_load_from_memory(
			(unsigned char*)(data.data()),
			data.size(),
			&width,
			&height,
			&texChannels,
			STBI_rgb_alpha
		);

		if (!pixels)
		{
			//LOG_ERROR("Failed to load texture! {}", filePath);
			return false;
		}

		std::memcpy(result, pixels, width * height * sizeof(char) * 4);

		stbi_image_free(pixels);
		return true;
	}
}
