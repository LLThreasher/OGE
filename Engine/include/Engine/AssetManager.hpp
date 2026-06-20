#pragma once

#include <unordered_map>
#include <string>
#include <entt/entt.hpp>
#include <queue>
#include "Engine/ObjectType.hpp"
#include "Engine/ResourcePool.hpp"
#include "Engine/Graphics/IGraphicsBackend.hpp"
#include "Engine/StreamingManager.hpp"
#include "Engine/Async.hpp"
#include "Engine/JobSystem.hpp"

namespace OneGame::Engine
{
	using StagingAllocation = Graphics::StagingAllocation;

	struct TextureData
	{
		int width;
		int height;
		std::vector<char> data;
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
		size_t vOffset;
		GPUBufferHandle indexBuffer;
		size_t iOffset;
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
	void AllocateMesh(const size_t vertBufSize, const size_t indexBufSize, Graphics::IGraphicsBackend* backend, Mesh& m);

	struct TextureInfo
	{
		uint32_t width;
		uint32_t height;
	};

	class AssetManager;

	template<UploadType uploadType>
	struct AssetBundleWriter
	{
	public:
		AssetManager* m_assetManager;
		StreamingManager* m_streamingManager;
		Graphics::IGraphicsBackend* m_backend;

		AssetBundleWriter(AssetManager* assets, StreamingManager* stream, Graphics::IGraphicsBackend* backend) :
			m_assetManager(assets), m_streamingManager(stream), m_backend(backend)
		{
			if constexpr (uploadType == UploadType::Async)
			{
				m_event = stream->CreateResourceBundle();
			}
		}
		~AssetBundleWriter() = default;

		bool LoadBlob(const std::string_view& id, std::vector<char>& data);

		GPUTextureHandle LoadTexture(const std::string_view& id)
		{
			using namespace Graphics;
			TextureInfo texInfo;
			m_assetManager->GetTextureInfo(id, texInfo);

			TextureDesc texDesc{};
			texDesc.width = texInfo.width;
			texDesc.height = texInfo.height;
			texDesc.format = TextureFormat::RGBA8Unorm;
			texDesc.usage = TextureUsage::TransferDst | TextureUsage::Sampled;
			GPUTextureHandle res = m_backend->CreateTexture(texDesc);

			auto data = m_assetManager->LoadTexture(id);
			//m_streamingManager->UploadTexture<uploadType>(std::span(data->data), res, 0, m_event);

			return res;
		}

		Mesh AllocateMesh(int vCount, int iCount)
		{
			Mesh m{};
			m.vertexBuffer = m_backend->AllocateGPUBuffer<BufferUsage::Vertex>(vCount);
			m.indexBuffer = m_backend->AllocateGPUBuffer<BufferUsage::Index>(iCount);
			return m;
		}

		template<typename TVertex, typename TIndex>
		Mesh LoadMesh(const std::vector<TVertex> vertices, const std::vector<TIndex> indices)
		{
			auto vCount = vertices.size() * sizeof(TVertex);
			auto iCount = indices.size() * sizeof(TIndex);
			Mesh m = AllocateMesh(vCount, iCount);
			UploadMesh(vertices, indices, m);
			return m;
		}

		template<typename TVertex, typename TIndex>
		void UploadMesh(const std::vector<TVertex> vertices, const std::vector<TIndex> indices, Mesh& m)
		{
			m_streamingManager->UploadBuffer<UploadType::Immediate, BufferUsage::Vertex>(std::span(vertices), m.vertexBuffer, m.vOffset);
			m_streamingManager->UploadBuffer<UploadType::Immediate, BufferUsage::Index>(std::span(indices), m.indexBuffer, m.iOffset);
			m.indexCount = indices.size();
		}

	private:
		ResourceBundleHandle m_event = {};
	};

	class AssetManager
	{
	public:
		AssetManager(AsyncDispatcher& dispatcher, JobSystem& jobSystem) : dispatcher(dispatcher), jobSystem(jobSystem) {}
		bool GetTextureInfo(const std::string_view& id, TextureInfo& info);
		TextureData* LoadTexture(const std::string_view& id);
		MeshData* LoadMesh(const std::string_view& id);

		Future<TextureData*> LoadTextureAsync(const std::string_view& id);
		Future<MeshData*> LoadMeshAsync(const std::string_view& id);

		void IssueLoads();
	private:
		AsyncDispatcher& dispatcher;
		JobSystem& jobSystem;
		std::queue <std::tuple<const std::string, Future<TextureData*>>> m_texturesToLoad;

		std::unordered_map<std::string, TextureData> m_textures;
		std::unordered_map<std::string, Mesh> m_meshes;
	};
}
