#include "Engine/AssetBundle.hpp"
#include "Engine/GameAppState.hpp"

#include "Engine/AssetManager.hpp"
#include "Engine/Graphics/IGraphicsBackend.hpp"
#include "Engine/StreamingManager.hpp"

namespace OneGame::Engine
{
using BufferUsage = Graphics::BufferUsage;

bool AssetPool::Load(const std::string_view& id, GPUTextureHandle& outTexture)
{
    auto it = m_texturePool.find(std::string(id));
    if (it == m_texturePool.end())
    {
        outTexture = {};
        return false;
    }
    else
    {
        outTexture = it->second;
        return true;
    }
}

void AssetPool::Cache(const std::string_view& id, const GPUTextureHandle texture)
{
    m_texturePool.emplace(id, texture);
}

bool AssetBase::LoadBlob(const std::string_view& id, std::vector<char>& data) { return TryLoadBlob(id, data); }

GPUTextureHandle AssetContext::LoadTexture(const std::string_view& id)
{
    GPUTextureHandle result;
    if (assetPool.Load(id, result))
    {
        return result;
    }
    auto tex = assetManager.LoadTexture(id);
    auto& info = tex->info;
    auto res = backend.AllocateGPUTexture(info.width, info.height);
    streamingManager.UploadTexture<UploadType::Immediate>(tex->data, res, 0);
    assetPool.Cache(id, res);
    return res;
}

Mesh AssetContext::LoadMesh(const std::span<const std::byte> vertices, const std::span<const std::byte> indices,
                         uint32_t indexCount, ResourceBundleHandle res)
{
    Mesh m = AllocateMesh(vertices.size(), indices.size());
    UploadMesh<UploadType::Immediate>(vertices, indices, m, indexCount, res);
    return m;
}

template <auto uploadType>
void AssetContext::UploadMesh(const std::span<const std::byte> vertices, const std::span<const std::byte> indices, Mesh& m,
                           uint32_t indexCount, ResourceBundleHandle res)
{
    streamingManager.UploadBuffer<uploadType, BufferUsage::Vertex>(vertices, m.vertexBuffer, m.vOffset, res);
    streamingManager.UploadBuffer<uploadType, BufferUsage::Index>(indices, m.indexBuffer, m.iOffset, res);
    m.indexCount = indexCount;
}

Mesh AssetContext::AllocateMesh(int vCount, int iCount)
{
    Mesh m{};
    m.vertexBuffer = backend.AllocateGPUBuffer<BufferUsage::Vertex>(vCount);
    m.indexBuffer = backend.AllocateGPUBuffer<BufferUsage::Index>(iCount);
    return m;
}

template void AssetContext::UploadMesh<UploadType::Async>(const std::span<const std::byte> vertices,
                                                       const std::span<const std::byte> indices, Mesh& m,
                                                       uint32_t indexCount, ResourceBundleHandle res);
template void AssetContext::UploadMesh<UploadType::Immediate>(const std::span<const std::byte> vertices,
                                                           const std::span<const std::byte> indices, Mesh& m,
                                                           uint32_t indexCount, ResourceBundleHandle res);
}  // namespace OneGame::Engine
