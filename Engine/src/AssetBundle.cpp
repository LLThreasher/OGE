#include "Engine/Graphics/IGraphicsBackend.hpp"
#include "Engine/StreamingManager.hpp"
#include "Engine/AssetManager.hpp"
#include "Engine/AssetBundle.hpp"

namespace OneGame::Engine
{
	using BufferUsage = Graphics::BufferUsage;

	AssetBundleWriter::AssetBundleWriter(AssetManager& assets, StreamingManager& stream, Graphics::IGraphicsBackend& backend) :
		m_assetManager(assets), m_streamingManager(stream), m_backend(backend)
	{

	}
	bool AssetBundleWriter::LoadBlob(const std::string_view& id, std::vector<char>& data)
	{
		return TryLoadBlob(id, data);
	}

	GPUTextureHandle AssetBundleWriter::LoadTexture(const std::string_view& id)
	{
		auto tex = m_assetManager.LoadTexture(id);
		auto& info = tex->info;
		auto res = m_backend.AllocateGPUTexture(info.width, info.height);
		m_streamingManager.UploadTexture<UploadType::Immediate>(tex->data, res, 0);
		return res;
	}

	Mesh AssetBundleWriter::LoadMesh(const std::span<const std::byte> vertices, const std::span<const std::byte> indices, uint32_t indexCount)
	{
		Mesh m = AllocateMesh(vertices.size(), indices.size());
		UploadMesh(vertices, indices, m, indexCount);
		return m;
	}

	void AssetBundleWriter::UploadMesh(const std::span<const std::byte> vertices, const std::span<const std::byte> indices, Mesh& m, uint32_t indexCount)
	{
		m_streamingManager.UploadBuffer<UploadType::Immediate, BufferUsage::Vertex>(vertices, m.vertexBuffer, m.vOffset);
		m_streamingManager.UploadBuffer<UploadType::Immediate, BufferUsage::Index>(indices, m.indexBuffer, m.iOffset);
		m.indexCount = indexCount;
	}

	Mesh AssetBundleWriter::AllocateMesh(int vCount, int iCount)
	{
		Mesh m{};
		m.vertexBuffer = m_backend.AllocateGPUBuffer<BufferUsage::Vertex>(vCount);
		m.indexBuffer = m_backend.AllocateGPUBuffer<BufferUsage::Index>(iCount);
		return m;
	}
}
