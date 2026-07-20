#pragma once
#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "oge/macros.hpp"
#include "oge/math.hpp"
#include "oge/graphics/objects.hpp"
#include "oge/point2.hpp"
#include "oge/graphics/configs.hpp"


#ifndef MAX_FRAMES_IN_FLIGHT
#define MAX_FRAMES_IN_FLIGHT 1
#endif

#ifndef MAX_CMD_BUFFER_PER_QUEUE
#define MAX_CMD_BUFFER_PER_QUEUE 1
#endif

namespace oge::platform
{
    struct WindowHandle;
}

namespace oge::graphics
{
class ICommandList;
using WindowHandle = platform::WindowHandle;

enum class FrameTimePreference
{
    VSync,
    Unlimited,
};

struct BackendDesc
{
    WindowHandle* window;
    FrameTimePreference frameTime = FrameTimePreference::VSync;
};

struct BufferDesc
{
    uint64_t size = 0;
    BufferUsage usage = BufferUsage::None;
    MemoryUsage memory = MemoryUsage::GPUOnly;
};

struct TextureDesc
{
    uint32_t width = 1;
    uint32_t height = 1;
    uint32_t depth = 1;
    uint32_t mipLevels = 1;
    uint32_t layers = 1;

    TextureFormat format = TextureFormat::BGRA8Unorm;
    TextureUsage usage = TextureUsage::Sampled;
};

struct PushConstantRangeDesc
{
    uint32_t offset = 0;
    uint32_t size = 0;
    ShaderStage stageFlags;
};

struct GraphicsPipelineDesc
{
    std::vector<char> vertexShader;
    std::vector<char> fragmentShader;
    std::vector<VertexAttributeFormat> vertexLayout;
    PrimitiveTopology topology = PrimitiveTopology::TriangleList;
    bool depthTest = false;
    bool writeDepth = false;
    DepthCompareOp depthCompareOp = DepthCompareOp::Never;
    bool blending = false;
    CullMode cullMode = CullMode::None;
    FrontFace frontFace = FrontFace::CW;

    std::vector<PushConstantRangeDesc> pushConstants;
    std::vector<GPUBindingGroupLayoutHandle> bindingGroupLayouts;
};

struct ComputePipelineDesc
{
    std::vector<char> shader;
    std::vector<GPUBindingGroupLayoutHandle> bindingGroupLayouts;
};

struct ColorAttachmentDesc
{
    TextureFormat format;

    LoadOp loadOp = LoadOp::Clear;
    StoreOp storeOp = StoreOp::Store;

    bool isSwapchain = false;
};

struct DepthAttachmentDesc
{
    TextureFormat format;

    LoadOp loadOp = LoadOp::Clear;
    StoreOp storeOp = StoreOp::DontCare;
};

struct RenderPassDesc
{
    std::array<ColorAttachmentDesc, MaxColorAttachments> colors;
    uint32_t colorCount = 0;

    bool hasDepth = false;
    DepthAttachmentDesc depth;
};

struct FrameBufferDesc
{
    std::array<GPUTextureHandle, MaxColorAttachments> colors;
    uint32_t colorCount = 0;
    bool hasDepth = false;
    GPUTextureHandle depth;
};

struct BindingGroupLayoutDesc
{
    int textureCount;
    int bufferCount;
    int dynamicBufferMask;
    int storageBufferMask;
};

struct BindingGroupBufferDesc
{
    GPUBufferHandle gpuBuffer;
    uint32_t stride = 0;

    BindingGroupBufferDesc(GPUBufferHandle gpuBuffer) : gpuBuffer(gpuBuffer), stride(0) {}

    BindingGroupBufferDesc(GPUBufferHandle gpuBuffer, uint32_t stride) : gpuBuffer(gpuBuffer), stride(stride) {}
};

struct BindingGroupDesc
{
    GPUBindingGroupLayoutHandle layout;
    std::vector<GPUTextureHandle> textures;
    std::vector<BindingGroupBufferDesc> buffers;
};

enum class BackendType
{
    Vulkan,
};

struct GPUInfo
{
    std::string name;
    int heapCount;
};

struct GPUMemoryUsage
{
    std::array<uint32_t, 16> heapUsage;
    std::array<uint32_t, 16> heapBudget;
};

enum class BeginFrameAction : uint32_t
{
    Continue,
    SkipFrame,
    RecreateSwapchain,
    RecreateSurface,
};

enum class EndFrameAction : uint32_t
{
    Continue,
    RecreateSwapchain,
    RecreateSurface,
};

class IGraphicsBackend
{
   public:
    IGraphicsBackend() {}
    NO_COPY(IGraphicsBackend)
    virtual ~IGraphicsBackend() = default;

    virtual uint32_t MaxUniformBufferSize() const = 0;
    virtual uint32_t UniformBufferAlignment() const = 0;

    virtual uint32_t FramesInFlight() const = 0;
    virtual uint32_t CurrentFrameIndex() const = 0;

    virtual float SwapchainAspect() const = 0;
    virtual U16Point2 SwapchainExtent() const = 0;
    virtual math::Orientation SwapchainPretransform() const = 0;
    virtual bool SwapchainRecreated() const = 0;

    virtual GPURenderPassHandle GetCurrentRenderPass() const = 0;
    virtual GPUFrameBufferHandle GetCurrentFrameBuffer() const = 0;

    virtual GPUInfo GetGPUInfo() const = 0;
    virtual GPUMemoryUsage GetGPUMemoryUsage() const = 0;

    virtual void Initialize(const BackendDesc&) = 0;
    virtual void RecreateSurface(WindowHandle* handle) = 0;
    virtual void Resize(uint32_t width, uint32_t height) = 0;
    virtual void WaitDeviceIdle() = 0;
    virtual void Shutdown() = 0;

    virtual BeginFrameAction BeginFrame() = 0;
    virtual EndFrameAction EndFrame() = 0;

    virtual ICommandList& CreateCommandList(QueueType) = 0;

    // ----- Buffers -----
    virtual GPUBufferHandle CreateBuffer(const BufferDesc&, void** = nullptr) = 0;
    virtual void DestroyBuffer(GPUBufferHandle) = 0;
    virtual void FlushStagingBufferRanges(const std::span<GPUBufferSpan> ranges) = 0;

    template <BufferUsage usage>
    GPUBufferHandle AllocateGPUBuffer(const size_t size)
    {
        BufferDesc iBuf{};
        iBuf.usage = usage | BufferUsage::TransferDst;
        iBuf.memory = MemoryUsage::GPUOnly;
        iBuf.size = size;
        return this->CreateBuffer(iBuf);
    }

    // ----- Textures -----
    virtual GPUTextureHandle CreateTexture(const TextureDesc&) = 0;
    virtual void DestroyTexture(GPUTextureHandle) = 0;

    template <TextureFormat format = TextureFormat::RGBA8Unorm>
    GPUTextureHandle AllocateGPUTexture(uint32_t width, uint32_t height, uint32_t layers = 1)
    {
        TextureDesc texDesc{};
        texDesc.width = width;
        texDesc.height = height;
        texDesc.format = format;
        texDesc.layers = layers;
        texDesc.usage = TextureUsage::TransferDst | TextureUsage::Sampled;
        return CreateTexture(texDesc);
    }

    // ----- Pipelines -----
    virtual GPUPipelineHandle CreateGraphicsPipeline(const GraphicsPipelineDesc&) = 0;
    virtual GPUPipelineHandle CreateComputePipeline(const ComputePipelineDesc&) = 0;
    virtual void DestroyPipeline(GPUPipelineHandle) = 0;

    // ----- Binding groups -----
    virtual GPUBindingGroupLayoutHandle CreateBindingGroupLayout(const BindingGroupLayoutDesc&) = 0;
    virtual void DestroyBindingGroupLayout(GPUBindingGroupLayoutHandle) = 0;

    virtual GPUBindingGroupHandle CreateBindingGroup(const BindingGroupDesc&) = 0;
    virtual void DestroyBindingGroup(GPUBindingGroupHandle) = 0;

    // ----- Sync -----
    virtual GPUFenceHandle CreateFence(bool signaled = false) = 0;
    virtual void WaitForFence(GPUFenceHandle) = 0;
    virtual bool IsFenceSignaled(GPUFenceHandle) = 0;
    virtual void ResetFence(GPUFenceHandle) = 0;

    virtual GPURenderPassHandle CreateRenderPass(const RenderPassDesc&) = 0;
    virtual void DestroyRenderPass(GPURenderPassHandle) = 0;

    virtual GPUFrameBufferHandle CreateFrameBuffer(GPURenderPassHandle passHandle, const FrameBufferDesc&) = 0;
    virtual void DestroyFrameBuffer(GPUFrameBufferHandle) = 0;
};

}  // namespace OneGame::Engine::Graphics
