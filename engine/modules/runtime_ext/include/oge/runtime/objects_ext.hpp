#pragma once

#include "oge/graphics/objects.hpp"

namespace oge::runtime
{
struct StagingAllocation : BufferSpan
{
    void* cpuPtr;
};

struct GPUChunkedAllocation
{
    uint16_t chunkSizeIdx;
    uint16_t blockIdx;
    uint32_t slotOffset;
};

struct GPUMesh
{
    GPUBufferHandle vertexBuffer;
    uint32_t vOffset;
    GPUBufferHandle indexBuffer;
    uint32_t iOffset;
    uint32_t indexCount;
};

struct ResourceBundle
{
    size_t itemCounter = 0;
};

enum class StreamingObjects
{
    ResourceBundle,
};

using ResourceBundleHandle = Handle<StreamingObjects::ResourceBundle>;

struct PSprite
{
    U16NormRect uv = {{0.f, 0.f}, {1.f, 1.f}};
    GPUTextureHandle texture;

    PSprite(GPUTextureHandle texture) : texture(texture) {}

    PSprite(GPUTextureRegion region, uint32_t total_width, uint32_t total_height)
    {
        float fwidth = total_width;
        float fheight = total_height;
        uv = {{(float)region.region.pos.x / fwidth, (float)region.region.pos.y / fheight},
              {(float)region.region.extent.x / fwidth, (float)region.region.extent.y / fheight}};
        texture = region.texture;
    }
};
}