#pragma once

#include <unordered_map>
#include <string>
#include <entt/entt.hpp>
#include <queue>
#include "Engine/ObjectType.hpp"
#include "Engine/ResourcePool.hpp"
#include "Engine/Graphics/IGraphicsBackend.hpp"
#include "Engine/Graphics/RingStagingBuffer.hpp"

namespace OneGame::Engine
{
	using StagingAllocation = Graphics::StagingAllocation;

	struct CPUTexture
	{
		int width;
		int height;
		StagingAllocation data;
	};

	enum class ShaderType
	{
		VulkanBinary
	};

	struct CPUShader
	{
		std::vector<char> data;
		ShaderType type;
	};

	struct Mesh
	{
		size_t indexCount;
		GPUBufferHandle vertexBuffer;
		GPUBufferHandle indexBuffer;
	};

	struct CPUMesh
	{
		size_t vertexBufSize;
		size_t indexBufSize;
		StagingAllocation vertexData;
		StagingAllocation indexData;
	};

	bool TryLoadPNG(std::vector<char> data, int& width, int& height, void* result);
	bool TryLoadBlob(const std::string_view& id, std::vector<char>&);
	void AllocateMesh(const size_t vertBufSize, const size_t indexBufSize, Graphics::IGraphicsBackend* backend, Mesh& m);

	class AssetManager
	{
	public:
		void StageUpload(const Graphics::IGraphicsBackend* backend, Graphics::RingStagingBuffer& stagingBuffer, Graphics::ICommandList* transferCmd);
		bool LoadTexture(const std::string_view& id, Graphics::RingStagingBuffer& stagingBuffer, Graphics::IGraphicsBackend* backend, GPUTextureHandle& outTexture);
		bool LoadMesh(const std::string_view& id, Graphics::RingStagingBuffer& stagingBuffer, Graphics::IGraphicsBackend* backend, Mesh& outMesh);
		bool LoadShader(const std::string_view& id, std::vector<char>& outShader);

	private:
		bool LoadMesh(Graphics::RingStagingBuffer& stagingBuffer, Graphics::IGraphicsBackend* backend, const void* vertices, const size_t vertexBufSize, const void* indices, const size_t indexBufSize, Mesh& outMesh);

		std::unordered_map<std::string, GPUTextureHandle> m_textures;
		std::queue<std::tuple<GPUTextureHandle, CPUTexture>> m_texturesToUpload;
		std::unordered_map<std::string, Mesh> m_meshes;
		std::queue<std::tuple<Mesh, CPUMesh>> m_meshesToUpload;
		std::array<std::queue<StagingAllocation>, 4> m_stagingBuffersToFree;
	};
}
