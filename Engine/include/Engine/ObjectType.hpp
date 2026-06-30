#pragma once

#include <cstddef>
#include <cstdint>

namespace OneGame::Engine
{
struct ColorRGBA8
{
    uint8_t r, g, b, a;

    uint32_t AsInt32() { return (uint32_t(r) << 0) | (uint32_t(g) << 8) | (uint32_t(b) << 16) | (uint32_t(a) << 24); }
};

constexpr ColorRGBA8 COLOR_WHITE = {255, 255, 255, 255};

enum class GPUObjectType : uint32_t
{
    Buffer,
    Texture,
    Pipeline,
    BindingGroupLayout,
    BindingGroup,
    Fence,
    QueryPool,
    RenderPass,
    FrameBuffer,
};

enum class AssetType : uint32_t
{
    // CPU side
    Texture,
    Shader,
    Material,
    Mesh,
};

enum class TempItem : uint32_t
{
    Job,
    StreamingDoneEvent,
};

template <auto Tag>
struct ResourceHandle
{
    uint32_t index = 0;
    uint32_t generation = 0;

    constexpr bool IsValid() const noexcept { return index != 0; }

    static ResourceHandle<Tag> InvalidHandle() { return ResourceHandle<Tag>{0, 0}; }

    bool operator==(const ResourceHandle<Tag>& other) const noexcept
    {
        return index == other.index && generation == other.generation;
    }
};

using GPUBufferHandle = ResourceHandle<GPUObjectType::Buffer>;
using GPUTextureHandle = ResourceHandle<GPUObjectType::Texture>;
using GPUPipelineHandle = ResourceHandle<GPUObjectType::Pipeline>;
using GPUBindingGroupHandle = ResourceHandle<GPUObjectType::BindingGroup>;
using GPUBindingGroupLayoutHandle = ResourceHandle<GPUObjectType::BindingGroupLayout>;
using GPUFenceHandle = ResourceHandle<GPUObjectType::Fence>;
using GPURenderPassHandle = ResourceHandle<GPUObjectType::RenderPass>;
using GPUFrameBufferHandle = ResourceHandle<GPUObjectType::FrameBuffer>;
using GPUQueryPoolHandle = ResourceHandle<GPUObjectType::QueryPool>;

using TextureHandle = ResourceHandle<AssetType::Texture>;
using ShaderHandle = ResourceHandle<AssetType::Shader>;
using MeshHandle = ResourceHandle<AssetType::Mesh>;

using JobHandle = ResourceHandle<TempItem::Job>;
using StreamingDoneEventHandle = ResourceHandle<TempItem::StreamingDoneEvent>;

enum class StreamingObjects
{
    ResourceBundle,
};

using ResourceBundleHandle = ResourceHandle<StreamingObjects::ResourceBundle>;

template <typename Handle>
struct HandleHash
{
    size_t operator()(const Handle& slot) const noexcept
    {
        return (static_cast<uint64_t>(slot.index) << 32) | slot.generation;
    }
};

struct GPUBufferSpan
{
    uint32_t offset;
    uint32_t size;
};

struct StagingAllocation : GPUBufferSpan
{
    void* cpuPtr;
};

struct GPUChunkedAllocation
{
    uint16_t chunkSizeIdx;
    uint16_t blockIdx;
    uint32_t slotOffset;
};

struct GPUBufferRange : GPUBufferSpan
{
    GPUBufferHandle buffer;
};

struct Mesh
{
    GPUBufferHandle vertexBuffer;
    uint32_t vOffset;
    GPUBufferHandle indexBuffer;
    uint32_t iOffset;
    uint32_t indexCount;
};
}  // namespace OneGame::Engine
