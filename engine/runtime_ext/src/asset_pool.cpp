#include "oge/runtime/asset_pool.hpp"

#include "oge/runtime/asset_manager.hpp"
#include "oge/runtime/asset_ctx.hpp"
#include "oge/graphics/backend.hpp"
#include "oge/runtime/streaming_manager.hpp"
#include "oge/platform/io.hpp"
#include "oge/runtime/objects_ext.hpp"
#include "oge/runtime/ui/objects.hpp"

namespace oge::runtime
{
using namespace graphics;
using namespace platform;

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

bool AssetPool::Load(const std::string_view& id, std::shared_ptr<ui::IFont>& font)
{
    auto it = m_fontPool.find(std::string(id));
    if (it == m_fontPool.end())
    {
        font = {};
        return false;
    }
    else
    {
        font = it->second;
        return true;
    }
}

void AssetPool::Cache(const std::string_view& id, const std::shared_ptr<ui::IFont> font)
{
    m_fontPool.emplace(id, font);
}

bool AssetBase::LoadBlob(const std::string_view& id, std::vector<char>& data) { return TryLoadBlob(id, data); }

std::shared_ptr<ui::IFont> AssetContext::LoadASCIIBitmapFont16x6(const std::string_view& id)
{
    std::shared_ptr<ui::IFont> ptr;
    if (assetPool.Load(id, ptr))
    {
        return ptr;
    }
    auto unique = ui::LoadASCIIBitmapFont16x6(*this, std::string(id));
    ptr = std::shared_ptr<ui::IFont>(unique.release());
    assetPool.Cache(id, ptr);
    return ptr;
}

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
    streamingManager.UploadTexture<UploadType::Immediate>(tex->data, {res, {{0, 0}, {info.width, info.height}}});
    assetPool.Cache(id, res);
    return res;
}

GPUMesh AssetContext::LoadMesh(const std::span<const std::byte> vertices, const std::span<const std::byte> indices,
                            uint32_t indexCount, ResourceBundleHandle res)
{
    GPUMesh m = AllocateMesh(vertices.size(), indices.size());
    UploadMesh<UploadType::Immediate>(vertices, indices, m, indexCount, res);
    return m;
}

template <auto uploadType>
void AssetContext::UploadMesh(const std::span<const std::byte> vertices, const std::span<const std::byte> indices,
                              GPUMesh& m, uint32_t indexCount, ResourceBundleHandle res)
{
    streamingManager.Upload<uploadType>(
        vertices, BufferTarget{.usage = BufferUsage::Vertex, .buffer = m.vertexBuffer, .offset = m.vOffset}, res);
    streamingManager.Upload<uploadType>(
        indices, BufferTarget{.usage = BufferUsage::Index, .buffer = m.indexBuffer, .offset = m.iOffset}, res);
    m.indexCount = indexCount;
}

GPUMesh AssetContext::AllocateMesh(int vCount, int iCount)
{
    GPUMesh m{};
    m.vertexBuffer = backend.AllocateGPUBuffer<BufferUsage::Vertex>(vCount);
    m.indexBuffer = backend.AllocateGPUBuffer<BufferUsage::Index>(iCount);
    return m;
}

template void AssetContext::UploadMesh<UploadType::Async>(const std::span<const std::byte> vertices,
                                                          const std::span<const std::byte> indices, GPUMesh& m,
                                                          uint32_t indexCount, ResourceBundleHandle res);
template void AssetContext::UploadMesh<UploadType::Immediate>(const std::span<const std::byte> vertices,
                                                              const std::span<const std::byte> indices, GPUMesh& m,
                                                              uint32_t indexCount, ResourceBundleHandle res);
}  // namespace OneGame::Engine
