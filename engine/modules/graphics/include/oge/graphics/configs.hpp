#pragma once

#include <cinttypes>
#include <array>

#include "oge/handle.hpp"
#include "oge/flag_helper.hpp"

namespace oge::graphics
{
using namespace oge::flag_helper;
static constexpr size_t MaxColorAttachments = 4;

enum class BufferUsage : uint32_t
{
    None = 0,
    Vertex = 1 << 0,
    Index = 1 << 1,
    Uniform = 1 << 2,
    Storage = 1 << 3,
    Indirect = 1 << 4,
    TransferSrc = 1 << 5,
    TransferDst = 1 << 6,
};

enum class MemoryUsage
{
    GPUOnly,
    CPUToGPU,
    GPUToCPU
};

enum class ShaderStage : uint32_t
{
    Vertex = 1 << 0,
    Fragment = 1 << 1,
    Compute = 1 << 2,
};

enum class TextureUsage : uint32_t
{
    None = 0,
    Sampled = 1 << 0,
    Storage = 1 << 1,
    ColorAttachment = 1 << 2,
    DepthAttachment = 1 << 3,
    TransferSrc = 1 << 4,
    TransferDst = 1 << 5,
};

enum class TextureState
{
    Undefined,
    ColorAttachment,
    DepthAttachment,
    ShaderRead,
    Storage,
    TransferSrc,
    TransferDst,
    Present
};

enum class TextureFormat
{
    Unknown,
    RGBA8Unorm,
    RGBA8Srgb,
    BGRA8Unorm,
    BGRA8Srgb,
    R8Unorm,
    R16Float,
    RG16Float,
    RGBA16Float,
    R32Float,
    RG32Float,
    RGBA32Float,
    Depth24FloatStencil8,
    Depth32Float,
    Depth32FloatStencil8,
};

enum class VertexAttributeFormat
{
    Float32,
    Float32x2,
    Float32x3,
    Float32x4,
    Uint32,
    Uint32x2,
    Uint32x3,
    Uint32x4,
    Uint16,
    Uint16x2,
    Uint8,
    UniformUint16,
    UniformUint16x2,
    UniformUint8x4,
};

enum class IndexFormat
{
    Uint32,
    Uint16,
};

enum class DepthCompareOp
{
    Never,
    Less,
    Equal,
    LessEqual,
    Greater,
    NotEqual,
    GreaterEqual,
    Always,
};

enum class CullMode
{
    None,
    Front,
    Back,
};

enum class FrontFace
{
    CW,
    CCW,
};

enum class PrimitiveTopology
{
    TriangleList,
    LineList,
};

enum class QueueType : uint32_t
{
    Present = 0,
    Transfer = 1,
    Graphics = 2,
    Compute = 3
};

enum class LoadOp
{
    Load,
    Clear,
    DontCare
};

enum class StoreOp
{
    Store,
    DontCare
};

struct ClearValues
{
    std::array<std::array<float, 4>, MaxColorAttachments> colorClears;
    float depthClear = 1.0f;
    uint32_t stencilClear = 0;
};

struct CopyTextureTarget
{
    uint32_t offsetX = 0;
    uint32_t offsetY = 0;
    uint32_t baseTextureLayer = 0;
    uint32_t mipLevel = 0;
};

}