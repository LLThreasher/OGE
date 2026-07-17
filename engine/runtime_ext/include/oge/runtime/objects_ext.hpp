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

}