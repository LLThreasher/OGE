#include "Engine/AssetManager.hpp"
#include "stb_image.h"


namespace OneGame::Engine
{
	using namespace Graphics;

	bool AssetBundleWriter::LoadTexture(const std::string_view& id, GPUTextureHandle& outTexture)
	{
		auto it = m_assetManager->m_textures.find(std::string(id));
		if (it != m_assetManager->m_textures.end())
		{
			outTexture = it->second;
			return true;
		}

		std::vector<char> blob;
		assert(TryLoadBlob(id, blob));
		CPUTexture result;
		TryLoadPNG(blob, result.width, result.height, nullptr);
		result.data = m_streamingManager->GetStagingBuffer()->Allocate(result.width * result.height * sizeof(char) * 4 * 2);
		TryLoadPNG(blob, result.width, result.height, result.data.cpuPtr);

		Graphics::TextureDesc texDesc{};
		texDesc.width = result.width;
		texDesc.height = result.height;
		texDesc.format = Graphics::TextureFormat::RGBA8Unorm;
		texDesc.usage = Graphics::TextureUsage::TransferDst | Graphics::TextureUsage::Sampled;
		auto texture = m_backend->CreateTexture(texDesc);

		m_assetManager->m_textures.emplace(id, texture);
		TextureUploadDesc desc{};
		desc.bundleEvent = m_event;
		desc.gpuBuffer = texture;
		desc.gpuBufferOffset = 0;
		desc.stagingAlloc = result.data;
		m_streamingManager->ScheduleTextureUpload(desc, m_event == nullptr ? UploadType::Immediate : UploadType::Async);
		outTexture = texture;
		return true;
	}

	bool AssetBundleWriter::LoadShader(const std::string_view& id, std::vector<char>& outShader)
	{
		return TryLoadBlob(id, outShader);
	}

	void AllocateMesh(const size_t vertBufSize, const size_t indexBufSize, Graphics::IGraphicsBackend* backend, Mesh& m)
	{
		BufferDesc vBuf{};
		vBuf.usage = BufferUsage::Vertex | BufferUsage::TransferDst;
		vBuf.memory = MemoryUsage::GPUOnly;
		vBuf.size = vertBufSize;
		m.vertexBuffer = backend->CreateBuffer(vBuf);

		BufferDesc iBuf{};
		iBuf.usage = BufferUsage::Index | BufferUsage::TransferDst;
		iBuf.memory = MemoryUsage::GPUOnly;
		iBuf.size = indexBufSize;
		m.indexBuffer = backend->CreateBuffer(iBuf);
	}

	struct TestPassVertex
	{
		math::vec3 pos;
		math::vec3 color;
		math::vec2 uv;
	};

	const std::vector<TestPassVertex> test_vertices =
	{
		// FRONT (+Z)
		{{-1,-1, 1}, {1,1,1}, {0,0}},
		{{ 1,-1, 1}, {1,1,1}, {1,0}},
		{{ 1, 1, 1}, {1,1,1}, {1,1}},
		{{-1, 1, 1}, {1,1,1}, {0,1}},

		// BACK (-Z)
		{{ 1,-1,-1}, {1,1,1}, {0,0}},
		{{-1,-1,-1}, {1,1,1}, {1,0}},
		{{-1, 1,-1}, {1,1,1}, {1,1}},
		{{ 1, 1,-1}, {1,1,1}, {0,1}},

		// LEFT (-X)
		{{-1,-1,-1}, {1,1,1}, {0,0}},
		{{-1,-1, 1}, {1,1,1}, {1,0}},
		{{-1, 1, 1}, {1,1,1}, {1,1}},
		{{-1, 1,-1}, {1,1,1}, {0,1}},

		// RIGHT (+X)
		{{ 1,-1, 1}, {1,1,1}, {0,0}},
		{{ 1,-1,-1}, {1,1,1}, {1,0}},
		{{ 1, 1,-1}, {1,1,1}, {1,1}},
		{{ 1, 1, 1}, {1,1,1}, {0,1}},

		// TOP (+Y)
		{{-1, 1, 1}, {1,1,1}, {0,0}},
		{{ 1, 1, 1}, {1,1,1}, {1,0}},
		{{ 1, 1,-1}, {1,1,1}, {1,1}},
		{{-1, 1,-1}, {1,1,1}, {0,1}},

		// BOTTOM (-Y)
		{{-1,-1,-1}, {1,1,1}, {0,0}},
		{{ 1,-1,-1}, {1,1,1}, {1,0}},
		{{ 1,-1, 1}, {1,1,1}, {1,1}},
		{{-1,-1, 1}, {1,1,1}, {0,1}},
	};

	const std::vector<uint32_t> test_indices =
	{
		0,1,2, 2,3,0,        // front
		4,5,6, 6,7,4,        // back
		8,9,10, 10,11,8,     // left
		12,13,14, 14,15,12,  // right
		16,17,18, 18,19,16,  // top
		20,21,22, 22,23,20   // bottom
	};

	bool AssetBundleWriter::LoadMesh(const std::string_view& id, Mesh& outMesh)
	{
		auto it = m_assetManager->m_meshes.find(std::string(id));
		if (it != m_assetManager->m_meshes.end())
		{
			outMesh = it->second;
			return true;
		}

		if (id == "test_cube.obj")
		{
			outMesh.indexCount = test_indices.size();
			auto res = LoadMesh(test_vertices.data(), test_vertices.size() * sizeof(TestPassVertex), test_indices.data(), test_indices.size() * sizeof(uint32_t), outMesh);
			m_assetManager->m_meshes.emplace(id, outMesh);
			return true;
		}
		return false;
	}

	bool AssetBundleWriter::LoadMesh(const void* vertices, const size_t vertexBufSize, const void* indices, const size_t indexBufSize, Mesh& outMesh)
	{
		auto stagingBuffer = m_streamingManager->GetStagingBuffer();
		CPUMesh cpuMesh{};
		cpuMesh.vertexData = stagingBuffer->Allocate(vertexBufSize);
		cpuMesh.vertexBufSize = vertexBufSize;
		cpuMesh.indexData = stagingBuffer->Allocate(indexBufSize);
		cpuMesh.indexBufSize = indexBufSize;

		memcpy(cpuMesh.vertexData.cpuPtr, vertices, vertexBufSize);
		memcpy(cpuMesh.indexData.cpuPtr, indices, indexBufSize);

		AllocateMesh(vertexBufSize, indexBufSize, m_backend, outMesh);
		{
			BufferUploadDesc vdesc{};
			vdesc.bundleEvent = m_event;
			vdesc.bufferUsage = BufferUsage::Vertex;
			vdesc.effectiveBufferSize = vertexBufSize;
			vdesc.gpuBuffer = outMesh.vertexBuffer;
			vdesc.gpuBufferOffset = 0;
			vdesc.stagingAlloc = cpuMesh.vertexData;
			m_streamingManager->ScheduleBufferUpload(vdesc, m_event == nullptr ? UploadType::Immediate : UploadType::Async);
		}
		{
			BufferUploadDesc idesc{};
			idesc.bundleEvent = m_event;
			idesc.bufferUsage = BufferUsage::Index;
			idesc.effectiveBufferSize = indexBufSize;
			idesc.gpuBuffer = outMesh.indexBuffer;
			idesc.gpuBufferOffset = 0;
			idesc.stagingAlloc = cpuMesh.indexData;
			m_streamingManager->ScheduleBufferUpload(idesc, m_event == nullptr ? UploadType::Immediate : UploadType::Async);
		}
		return true;
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

	std::unique_ptr<AssetBundleWriter> AssetManager::CreateAssetBundle(StreamingManager* streamingManager, Graphics::IGraphicsBackend* backend)
	{
		return std::unique_ptr<AssetBundleWriter>(new AssetBundleWriter(this, streamingManager, backend, false));
	}

	std::unique_ptr<AssetBundleWriter> AssetManager::CreateAssetBundleAsync(StreamingManager* streamingManager, Graphics::IGraphicsBackend* backend)
	{
		return std::unique_ptr<AssetBundleWriter>(new AssetBundleWriter(this, streamingManager, backend, true));
	}
}
