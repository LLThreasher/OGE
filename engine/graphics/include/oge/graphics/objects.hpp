#pragma once

#include <cinttypes>

#include "oge/handle.hpp"
#include "oge/rect.hpp"

namespace oge
{

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

using GPUBufferHandle = Handle<GPUObjectType::Buffer>;
using GPUTextureHandle = Handle<GPUObjectType::Texture>;
using GPUPipelineHandle = Handle<GPUObjectType::Pipeline>;
using GPUBindingGroupHandle = Handle<GPUObjectType::BindingGroup>;
using GPUBindingGroupLayoutHandle = Handle<GPUObjectType::BindingGroupLayout>;
using GPUFenceHandle = Handle<GPUObjectType::Fence>;
using GPURenderPassHandle = Handle<GPUObjectType::RenderPass>;
using GPUFrameBufferHandle = Handle<GPUObjectType::FrameBuffer>;
using GPUQueryPoolHandle = Handle<GPUObjectType::QueryPool>;

struct GPUTextureRegion
{
    URect region;
    GPUTextureHandle texture;
};

struct BufferSpan
{
    uint32_t offset;
    uint32_t size;
};

struct GPUBufferSpan : BufferSpan
{
    GPUBufferHandle buffer;
};
}  // namespace oge::graphics
