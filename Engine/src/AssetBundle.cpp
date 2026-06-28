#include "Engine/AssetBundle.hpp"

#include "Engine/AssetManager.hpp"
#include "Engine/Graphics/IGraphicsBackend.hpp"
#include "Engine/StreamingManager.hpp"

namespace OneGame::Engine
{
using BufferUsage = Graphics::BufferUsage;

AssetPool::AssetPool(AssetManager& assets, StreamingManager& stream, Graphics::IGraphicsBackend& backend)
    : m_assetManager(assets), m_streamingManager(stream), m_backend(backend)
{
}

bool AssetPool::LoadBlob(const std::string_view& id, std::vector<char>& data) { return TryLoadBlob(id, data); }

GPUTextureHandle AssetPool::LoadTexture(const std::string_view& id)
{
    auto tex = m_assetManager.LoadTexture(id);
    auto& info = tex->info;
    auto res = m_backend.AllocateGPUTexture(info.width, info.height);
    m_streamingManager.UploadTexture<UploadType::Immediate>(tex->data, res, 0);
    return res;
}

Mesh AssetPool::LoadMesh(const std::span<const std::byte> vertices, const std::span<const std::byte> indices,
                         uint32_t indexCount, ResourceBundleHandle res)
{
    Mesh m = AllocateMesh(vertices.size(), indices.size());
    UploadMesh<UploadType::Immediate>(vertices, indices, m, indexCount, res);
    return m;
}

template <auto uploadType>
void AssetPool::UploadMesh(const std::span<const std::byte> vertices, const std::span<const std::byte> indices, Mesh& m,
                           uint32_t indexCount, ResourceBundleHandle res)
{
    m_streamingManager.UploadBuffer<uploadType, BufferUsage::Vertex>(vertices, m.vertexBuffer, m.vOffset, res);
    m_streamingManager.UploadBuffer<uploadType, BufferUsage::Index>(indices, m.indexBuffer, m.iOffset, res);
    m.indexCount = indexCount;
}

Mesh AssetPool::AllocateMesh(int vCount, int iCount)
{
    Mesh m{};
    m.vertexBuffer = m_backend.AllocateGPUBuffer<BufferUsage::Vertex>(vCount);
    m.indexBuffer = m_backend.AllocateGPUBuffer<BufferUsage::Index>(iCount);
    return m;
}

template void AssetPool::UploadMesh<UploadType::Async>(const std::span<const std::byte> vertices,
                                                       const std::span<const std::byte> indices, Mesh& m,
                                                       uint32_t indexCount, ResourceBundleHandle res);
template void AssetPool::UploadMesh<UploadType::Immediate>(const std::span<const std::byte> vertices,
                                                           const std::span<const std::byte> indices, Mesh& m,
                                                           uint32_t indexCount, ResourceBundleHandle res);
}  // namespace OneGame::Engine
