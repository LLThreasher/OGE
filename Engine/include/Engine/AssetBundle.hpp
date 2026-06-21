#pragma once

#include "Engine/ObjectType.hpp"

namespace OneGame::Engine
{
	class AssetManager;
	class StreamingManager;
	namespace Graphics
	{
		class IGraphicsBackend;
	}

	struct AssetBundleWriter
	{
	public:
		AssetBundleWriter(AssetManager& assets, StreamingManager& stream, Graphics::IGraphicsBackend& backend);
		~AssetBundleWriter() = default;

		bool LoadBlob(const std::string_view& id, std::vector<char>& data);

		GPUTextureHandle LoadTexture(const std::string_view& id);

		Mesh AllocateMesh(int vCount, int iCount);
		Mesh LoadMesh(const std::span<const std::byte> vertices, const std::span<const std::byte> indices, uint32_t indexCount);
		template<typename TData, typename TIndex>
		Mesh LoadMesh(const std::vector<TData>& vertices, const std::vector<TIndex>& indices)
		{
			return LoadMesh(std::as_bytes(std::span{ vertices }), std::as_bytes(std::span{ indices }), indices.size());
		}
		void UploadMesh(const std::span<const std::byte> vertices, const std::span<const std::byte> indices, Mesh& m, uint32_t indexCount);
		template<typename TData, typename TIndex>
		void UploadMesh(const std::vector<TData>& vertices, const std::vector<TIndex>& indices, Mesh& m)
		{
			UploadMesh(std::as_bytes(std::span{ vertices }), std::as_bytes(std::span{ indices }), m, indices.size());
		}

	private:
		AssetManager& m_assetManager;
		StreamingManager& m_streamingManager;
		Graphics::IGraphicsBackend& m_backend;
	};
}
