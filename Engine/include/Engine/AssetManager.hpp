#pragma once

#include <unordered_map>
#include <string>
#include "Engine/ObjectType.hpp"
#include "Engine/Math.hpp"

#include "Engine/ClassHelper.hpp"

namespace OneGame::Engine
{
	struct TextureInfo
	{
		uint32_t width;
		uint32_t height;
	};

	struct TextureData
	{
		TextureInfo info;
		std::vector<char> data;
	};

	struct Material
	{
		GPUBufferHandle gpuBuffer;
	};

	struct MeshData
	{
		std::vector<math::vec3> positions;
		std::vector<math::vec2> uvs;
		std::vector<uint32_t> indices;
	};

	bool TryLoadPNG(std::vector<char> data, int& width, int& height, void* result);
	bool TryLoadBlob(const std::string_view& id, std::vector<char>&);

	class AssetManager
	{
	public:
		NO_COPY(AssetManager)
		TextureInfo* GetTextureInfo(const std::string_view& id);
		TextureData* LoadTexture(const std::string_view& id);
		MeshData* LoadMesh(const std::string_view& id);
	private:
		std::unordered_map<std::string, TextureData> m_textures;
		std::unordered_map<std::string, Mesh> m_meshes;
	};
}
