#include "Engine/AssetManager.hpp"
#include "stb_image.h"


namespace OneGame::Engine
{
	using namespace Graphics;

	void AssetManager::StageUpload(const Graphics::IGraphicsBackend* backend, Graphics::RingStagingBuffer& stagingBuffer, Graphics::ICommandList* transferCmd)
	{
		auto fidx = backend->CurrentFrameIndex();
		while (!m_stagingBuffersToFree[fidx].empty())
		{
			auto& buffer = m_stagingBuffersToFree[fidx].back();
			stagingBuffer.Free(buffer.offset, buffer.size);
			m_stagingBuffersToFree[fidx].pop();
		}
		while (!m_texturesToUpload.empty())
		{
			auto& [gpuHandle, cpuTex] = m_texturesToUpload.back();
			transferCmd->TextureBarrier(gpuHandle, Graphics::TextureState::TransferDst);
			transferCmd->CopyBufferToTexture(stagingBuffer.GetBuffer(), gpuHandle, cpuTex.data.offset);
			transferCmd->TextureBarrier(gpuHandle, Graphics::TextureState::ShaderRead);

			m_stagingBuffersToFree[fidx].push(cpuTex.data);
			m_texturesToUpload.pop();
		}
		while (!m_meshesToUpload.empty())
		{
			auto& [mesh, cpuMesh] = m_meshesToUpload.back();
			transferCmd->CopyBuffer(stagingBuffer.GetBuffer(), mesh.vertexBuffer, cpuMesh.vertexBufSize, cpuMesh.vertexData.offset);
			transferCmd->CopyBuffer(stagingBuffer.GetBuffer(), mesh.indexBuffer, cpuMesh.indexBufSize, cpuMesh.indexData.offset);
			transferCmd->BufferBarrier(mesh.vertexBuffer, BufferUsage::Vertex | BufferUsage::TransferDst, BufferUsage::Vertex);
			transferCmd->BufferBarrier(mesh.indexBuffer, BufferUsage::Index | BufferUsage::TransferDst, BufferUsage::Index);

			m_stagingBuffersToFree[fidx].push(cpuMesh.vertexData);
			m_stagingBuffersToFree[fidx].push(cpuMesh.indexData);
			m_meshesToUpload.pop();
		}
	}

	bool AssetManager::LoadTexture(const std::string_view& id, Graphics::RingStagingBuffer& stagingBuffer, Graphics::IGraphicsBackend* backend, GPUTextureHandle& outTexture)
	{
		auto it = m_textures.find(std::string(id));
		if (it != m_textures.end())
		{
			outTexture = it->second;
			return true;
		}

		std::vector<char> blob;
		assert(TryLoadBlob(id, blob));
		CPUTexture result;
		TryLoadPNG(blob, result.width, result.height, nullptr);
		result.data = stagingBuffer.Allocate(result.width * result.height * sizeof(char) * 4 * 2);
		TryLoadPNG(blob, result.width, result.height, result.data.cpuPtr);

		Graphics::TextureDesc texDesc{};
		texDesc.width = result.width;
		texDesc.height = result.height;
		texDesc.format = Graphics::TextureFormat::RGBA8Unorm;
		texDesc.usage = Graphics::TextureUsage::TransferDst | Graphics::TextureUsage::Sampled;
		auto texture = backend->CreateTexture(texDesc);

		m_textures.emplace(id, texture);
		m_texturesToUpload.push({ texture, result });
		outTexture = texture;
		return true;
	}

	bool AssetManager::LoadShader(const std::string_view& id, std::vector<char>& outShader)
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

	bool AssetManager::LoadMesh(const std::string_view& id, Graphics::RingStagingBuffer& stagingBuffer, Graphics::IGraphicsBackend* backend, Mesh& outMesh)
	{
		auto it = m_meshes.find(std::string(id));
		if (it != m_meshes.end())
		{
			outMesh = it->second;
			return true;
		}

		if (id == "test_cube.obj")
		{
			outMesh.indexCount = test_indices.size();
			auto res = LoadMesh(stagingBuffer, backend, test_vertices.data(), test_vertices.size() * sizeof(TestPassVertex), test_indices.data(), test_indices.size() * sizeof(uint32_t), outMesh);
			m_meshes.emplace(id, outMesh);
			return true;
		}
		return false;
	}

	bool AssetManager::LoadMesh(Graphics::RingStagingBuffer& stagingBuffer, Graphics::IGraphicsBackend* backend, const void* vertices, const size_t vertexBufSize, const void* indices, const size_t indexBufSize, Mesh& outMesh)
	{
		CPUMesh cpuMesh{};
		cpuMesh.vertexData = stagingBuffer.Allocate(vertexBufSize);
		cpuMesh.vertexBufSize = vertexBufSize;
		cpuMesh.indexData = stagingBuffer.Allocate(indexBufSize);
		cpuMesh.indexBufSize = indexBufSize;

		memcpy(cpuMesh.vertexData.cpuPtr, vertices, vertexBufSize);
		memcpy(cpuMesh.indexData.cpuPtr, indices, indexBufSize);

		AllocateMesh(vertexBufSize, indexBufSize, backend, outMesh);
		m_meshesToUpload.push({ outMesh, cpuMesh });
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
}
