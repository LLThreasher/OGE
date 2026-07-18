#pragma once

#include <cinttypes>
#include <span>

#include "oge/graphics/objects.hpp"
#include "oge/runtime/asset_base.hpp"
#include "oge/runtime/objects_ext.hpp"

namespace oge::graphics
{
class IGraphicsBackend;
}  // namespace oge::graphics

namespace oge::runtime
{

class StreamingManager;
class AssetPool;

namespace ui
{
class IFont;
};

namespace gfx
{
namespace dca
{
class DynamicChunkAllocator;
}  // namespace dca

class DynamicSkylineAllocator;
}  // namespace renderer

using DynamicChunkAllocator = gfx::dca::DynamicChunkAllocator;
using DynamicSkylineAllocator = gfx::DynamicSkylineAllocator;

struct AssetContext : AssetBase
{
    StreamingManager& streamingManager;
    graphics::IGraphicsBackend& backend;
    AssetPool& assetPool;
    DynamicChunkAllocator& chunkAllocator;
    DynamicSkylineAllocator& spriteAllocator;

    GPUTextureHandle LoadTexture(const std::string_view& id);
    GPUMesh AllocateMesh(int vCount, int iCount);
    GPUMesh LoadMesh(const std::span<const std::byte> vertices, const std::span<const std::byte> indices,
                     uint32_t indexCount, ResourceBundleHandle res = {});
    template <typename TData, typename TIndex>
    GPUMesh LoadMesh(const std::vector<TData>& vertices, const std::vector<TIndex>& indices)
    {
        return LoadMesh(std::as_bytes(std::span{vertices}), std::as_bytes(std::span{indices}), indices.size());
    }
    template <auto uploadType>
    void UploadMesh(const std::span<const std::byte> vertices, const std::span<const std::byte> indices, GPUMesh& m,
                    uint32_t indexCount, ResourceBundleHandle res = {});
    template <auto uploadType, typename TData, typename TIndex>
    void UploadMesh(const std::vector<TData>& vertices, const std::vector<TIndex>& indices, GPUMesh& m,
                    ResourceBundleHandle res = {})
    {
        UploadMesh<uploadType>(std::as_bytes(std::span{vertices}), std::as_bytes(std::span{indices}), m, indices.size(),
                               res);
    }

    std::shared_ptr<ui::IFont> LoadASCIIBitmapFont16x6(const std::string_view& id);
};

}  // namespace oge::runtime
