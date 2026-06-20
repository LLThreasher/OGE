#include "Engine/AssetManager.hpp"
#include "stb_image.h"
#include "Engine/Terrain/TerrainVertexFormat.hpp"


namespace OneGame::Engine
{
	using namespace Graphics;

	bool AssetManager::GetTextureInfo(const std::string_view& id, TextureInfo& info)
	{
		info = {};
		auto str_id = std::string(id);
		auto it = m_textures.find(str_id);
		if (it != m_textures.end())
		{
			info.width = it->second.width;
			info.height = it->second.height;
			return true;
		}

		std::vector<char> blob;
		if (!TryLoadBlob(id, blob))
			return false;
		int iwidth, iheight;
		if (!TryLoadPNG(blob, iwidth, iheight, nullptr))
			return false;
		info.width = iwidth;
		info.height = iheight;
		return true;
	}

	TextureData* AssetManager::LoadTexture(const std::string_view& id)
	{
		auto str_id = std::string(id);
		auto it = m_textures.find(str_id);
		if (it != m_textures.end())
		{
			return &it->second;
		}

		TextureData data;
		std::vector<char> blob;
		if (!TryLoadBlob(id, blob))
			return nullptr;
		if (!TryLoadPNG(blob, data.width, data.height, nullptr))
			return nullptr;
		data.data.resize(data.width * data.height * sizeof(char) * 4 * 2);
		if (!TryLoadPNG(blob, data.width, data.height, data.data.data()))
			return nullptr;
		m_textures.emplace(id, data);
		return &m_textures[str_id];
	}

	Future<TextureData*> AssetManager::LoadTextureAsync(const std::string_view& id)
	{
		auto res = dispatcher.create_future<TextureData*>();
		m_texturesToLoad.emplace(std::string(id), res);
		return res;
	}

	void AssetManager::IssueLoads()
	{
		while (!m_texturesToLoad.empty())
		{
			auto [id, future] = std::move(m_texturesToLoad.front());
			m_texturesToLoad.pop();
			jobSystem.submit([&](auto canceled) mutable
				{
					future.post(this->LoadTexture(id));
				});
		}
	}

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
