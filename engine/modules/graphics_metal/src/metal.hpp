#pragma once

#include <Metal/Metal.hpp>
#include <QuartzCore/CAMetalLayer.hpp>

#include <cstdint>
#include <span>
#include <vector>

#include "oge/pool.hpp"
#include "oge/graphics/backend.hpp"

namespace oge::graphics::metal
{

class MetalCommandBuffer;

struct MetalDevice
{
    NS::SharedPtr<MTL::Device> device = nullptr;

    NS::SharedPtr<MTL::CommandQueue> graphicsQueue = nullptr;
    NS::SharedPtr<MTL::CommandQueue> computeQueue = nullptr;
    NS::SharedPtr<MTL::CommandQueue> transferQueue = nullptr;

    CA::MetalLayer* layer = nullptr;

    MTL::PixelFormat swapchainFormat = MTL::PixelFormatInvalid;
    MTL::PixelFormat depthFormat = MTL::PixelFormatInvalid;

    uint32_t maxFramesInFlight = 0;
};

struct MetalSwapchain
{
    math::Orientation currentTransform = math::Orientation::IDENTITY;

    CA::MetalLayer* layer = nullptr;
    CA::MetalDrawable* currentDrawable = nullptr;

    U16Point2 extent = {};
    U16Point2 nextExtent = {};

    GPUTextureHandle currentColorTexture = {};
    GPUTextureHandle currentDepthTexture = {};

    bool isDirty = false;
    bool wasRecreated = false;
};

struct MetalFrameData
{
    NS::SharedPtr<MTL::CommandBuffer> commandBuffer = nullptr;
    dispatch_semaphore_t inFlightSemaphore = nullptr;
};

struct MetalBuffer
{
    NS::SharedPtr<MTL::Buffer> buffer = nullptr;

    BufferDesc desc = {};
    void* mappedMemory = nullptr;
};

struct MetalTexture
{
    NS::SharedPtr<MTL::Texture> texture = nullptr;

    TextureDesc desc = {};

    bool isSwapchainTexture = false;
    bool isDepthTexture = false;
};

struct MetalPipeline
{
    NS::SharedPtr<MTL::RenderPipelineState> renderPipeline = nullptr;
    NS::SharedPtr<MTL::ComputePipelineState> computePipeline = nullptr;
    NS::SharedPtr<MTL::DepthStencilState> depthStencilState = nullptr;

    GraphicsPipelineDesc graphicsDesc = {};
    ComputePipelineDesc computeDesc = {};

    MTL::PrimitiveType primitiveType = MTL::PrimitiveTypeTriangle;
};

struct MetalBindingGroupLayout
{
    BindingGroupLayoutDesc desc = {};
};

struct MetalBindingGroup
{
    BindingGroupDesc desc = {};
};

struct MetalFence
{
    bool signaled = false;
    NS::SharedPtr<MTL::CommandBuffer> commandBuffer = nullptr;
};

class MetalBackend final : public IGraphicsBackend
{
    friend class MetalCommandBuffer;

   public:
    MetalBackend();
    ~MetalBackend() override;

    uint32_t MaxUniformBufferSize() const override;
    uint32_t UniformBufferAlignment() const override;

    uint32_t FramesInFlight() const override;
    uint32_t CurrentFrameIndex() const override;

    float SwapchainAspect() const override;
    U16Point2 SwapchainExtent() const override;
    math::Orientation SwapchainPretransform() const override;
    bool SwapchainRecreated() const override;

    GPUInfo GetGPUInfo() const override;
    GPUMemoryUsage GetGPUMemoryUsage() const override;

    void Initialize(const BackendDesc&) override;
    void RecreateSurface(WindowHandle& handle) override;
    void Resize(uint32_t width, uint32_t height) override;
    void WaitDeviceIdle() override;
    void Shutdown() override;

    BeginFrameAction BeginFrame() override;
    EndFrameAction EndFrame() override;

    ICommandList& CreateCommandList(QueueType) override;

    // ----- Buffers -----
    GPUBufferHandle CreateBuffer(const BufferDesc&,
                                 void** stagingMemory = nullptr) override;
    void DestroyBuffer(GPUBufferHandle) override;
    void FlushStagingBufferRanges(
        const std::span<GPUBufferSpan> ranges) override;

    // ----- Textures -----
    GPUTextureHandle CreateTexture(const TextureDesc&) override;
    void DestroyTexture(GPUTextureHandle) override;

    // ----- Pipelines -----
    GPUPipelineHandle CreateGraphicsPipeline(
        const GraphicsPipelineDesc&) override;
    GPUPipelineHandle CreateComputePipeline(
        const ComputePipelineDesc&) override;
    void DestroyPipeline(GPUPipelineHandle) override;

    // ----- Binding groups -----
    GPUBindingGroupLayoutHandle CreateBindingGroupLayout(
        const BindingGroupLayoutDesc&) override;
    void DestroyBindingGroupLayout(GPUBindingGroupLayoutHandle) override;

    GPUBindingGroupHandle CreateBindingGroup(const BindingGroupDesc&) override;
    void DestroyBindingGroup(GPUBindingGroupHandle) override;

    // ----- Sync -----
    GPUFenceHandle CreateFence(bool signaled = false) override;
    void WaitForFence(GPUFenceHandle) override;
    bool IsFenceSignaled(GPUFenceHandle) override;
    void ResetFence(GPUFenceHandle) override;

   private:
    MetalDevice m_device = {};
    MetalSwapchain m_swapchain = {};

    uint32_t m_frameIndex = 0;

    std::vector<MetalFrameData> m_frames = {};

    NS::SharedPtr<MTL::Library> m_defaultLibrary = nullptr;

    Pool<GPUObjectType::Buffer, MetalBuffer> m_buffers;
    Pool<GPUObjectType::Texture, MetalTexture> m_textures;
    Pool<GPUObjectType::Pipeline, MetalPipeline> m_pipelines;
    Pool<GPUObjectType::BindingGroupLayout, MetalBindingGroupLayout>
        m_bindingGroupLayouts;
    Pool<GPUObjectType::BindingGroup, MetalBindingGroup> m_bindingGroups;
    Pool<GPUObjectType::Fence, MetalFence> m_fences;

   private:
    bool CreateDevice();
    void CreateCommandQueues();

    void CreateSwapchain(WindowHandle* handle);
    void DestroySwapchain();
    bool RecreateSwapchain();

    void CreateFrameData(uint32_t framesInFlight);
    void DestroyFrameData();

    bool AcquireNextDrawable();

    GPUTextureHandle CreateSwapchainTexture(CA::MetalDrawable* drawable);
    GPUTextureHandle CreateDepthTexture(uint32_t width, uint32_t height);

    MTL::RenderPassDescriptor* CreateRenderPassDescriptor(
        const GPURenderPassDesc& desc,
        const ClearValues& clearValues);

    void DestroyTextureInternal(MetalTexture& texture);
    void DestroyBufferInternal(MetalBuffer& buffer);

    NS::SharedPtr<MTL::Function> CreateShaderFunction(
        const ShaderDesc& shaderDesc);

    NS::SharedPtr<MTL::DepthStencilState> CreateDepthStencilState(
        const GraphicsPipelineDesc& desc);

    MTL::RenderPipelineDescriptor* CreateRenderPipelineDescriptor(
        const GraphicsPipelineDesc& desc);

    MTL::ComputePipelineDescriptor* CreateComputePipelineDescriptor(
        const ComputePipelineDesc& desc);

    MTL::PixelFormat ToMetalPixelFormat(TextureFormat format) const;
    MTL::TextureUsage ToMetalTextureUsage(TextureUsage usage) const;
    MTL::ResourceOptions ToMetalResourceOptions(MemoryUsage usage) const;
    MTL::StorageMode ToMetalStorageMode(MemoryUsage usage) const;
    MTL::CPUCacheMode ToMetalCPUCacheMode(MemoryUsage usage) const;

    MTL::LoadAction ToMetalLoadAction(LoadOp op) const;
    MTL::StoreAction ToMetalStoreAction(StoreOp op) const;

    MTL::PrimitiveType ToMetalPrimitiveType(PrimitiveTopology topology) const;
    MTL::IndexType ToMetalIndexType(IndexFormat format) const;

    MTL::CompareFunction ToMetalCompareFunction(DepthCompareOp function) const;
    MTL::CullMode ToMetalCullMode(CullMode mode) const;
    MTL::Winding ToMetalWinding(FrontFace face) const;
};

CA::MetalLayer* RetainMetalLayer(WindowHandle* handle);
void ReleaseMetalLayer(WindowHandle* handle, CA::MetalLayer* layer);

}  // namespace oge::graphics::metal
