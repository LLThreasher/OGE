#pragma once

#include "oge/handle.hpp"
#include "oge/rect.hpp"

namespace oge::graphics
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

namespace gpu_objects
{
using oge::graphics::BufferSpan;
using oge::graphics::GPUBufferSpan;
using oge::graphics::GPUTextureRegion;

using oge::graphics::GPUBindingGroupHandle;
using oge::graphics::GPUBindingGroupLayoutHandle;
using oge::graphics::GPUBufferHandle;
using oge::graphics::GPUFenceHandle;
using oge::graphics::GPUFrameBufferHandle;
using oge::graphics::GPUPipelineHandle;
using oge::graphics::GPUQueryPoolHandle;
using oge::graphics::GPURenderPassHandle;
using oge::graphics::GPUTextureHandle;
}  // namespace gpu_objects
}  // namespace oge
