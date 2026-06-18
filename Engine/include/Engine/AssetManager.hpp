#pragma once

#include <unordered_map>
#include <string>
#include <entt/entt.hpp>
#include <queue>
#include "Engine/ObjectType.hpp"
#include "Engine/ResourcePool.hpp"
#include "Engine/Graphics/IGraphicsBackend.hpp"
#include "Engine/StreamingManager.hpp"

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

	struct Material
	{
		GPUBufferHandle gpuBuffer;
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

	class AssetManager;

	class AssetBundleWriter
	{
	public:
		AssetBundleWriter(AssetManager* assets, StreamingManager* stream, Graphics::IGraphicsBackend* backend, bool asyncLoad) :
			m_assetManager(assets), m_streamingManager(stream), m_backend(backend)
		{
			if (asyncLoad)
			{
				m_event = stream->CreateResourceBundle();
			}
		}
		~AssetBundleWriter() = default;
		bool LoadTexture(const std::string_view& id, GPUTextureHandle& outTexture);
		bool LoadMesh(const std::string_view& id, Mesh& outMesh);
		bool LoadShader(const std::string_view& id, std::vector<char>& outShader);
		bool LoadMaterial(const std::string_view& id, Material& outMaterial);

		std::shared_ptr<ResourceBundleEvent> GetOnLoadedEvent() { return m_event; }
	private:
		void LoadCpuMesh(const void* vertices, const size_t vertexBufSize, const void* indices, const size_t indexBufSize, CPUMesh& outMesh);
		bool LoadMesh(CPUMesh& cpuMesh, Mesh& outMesh);

		std::shared_ptr<ResourceBundleEvent> m_event = nullptr;
		AssetManager* m_assetManager;
		StreamingManager* m_streamingManager;
		Graphics::IGraphicsBackend* m_backend;
	};

	class AssetManager
	{
		friend class AssetBundleWriter;
	public:
		void AddMesh(const std::string& id, Mesh& mesh)
		{
			assert(m_meshes.find(id) == m_meshes.end());
			m_meshes.emplace(id, mesh);
		}

		std::unique_ptr<AssetBundleWriter> CreateAssetBundle(StreamingManager* streamingManager, Graphics::IGraphicsBackend* backend);
		std::unique_ptr<AssetBundleWriter> CreateAssetBundleAsync(StreamingManager* streamingManager, Graphics::IGraphicsBackend* backend);

	private:
		std::unordered_map<std::string, GPUTextureHandle> m_textures;
		std::unordered_map<std::string, Mesh> m_meshes;
	};
}
