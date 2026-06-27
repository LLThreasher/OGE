#pragma once
#include <cstdint>
#include <span>
#include <optional>
#include <array>
#include <vector>
#include <memory>
#include <string>

#include "Engine/ObjectType.hpp"
#include "Engine/Math.hpp"
#include "Engine/ClassHelper.hpp"

/*
================================================================================
Minimal GPU-Driven Voxel Frame Flow (Android / Vulkan-Oriented)
================================================================================

Assumptions:
- Chunk-based voxel world
- Each chunk has:
    - Bounding box
    - Mesh vertex/index buffers (generated CPU-side)
- GPU-driven = GPU performs visibility + writes indirect draw commands
- Rendering uses vkCmdDrawIndexedIndirect (or equivalent RHI call)

Per-Frame Overview:
CPU does minimal work.
GPU performs culling and builds draw list.

--------------------------------------------------------------------------------
FRAME STRUCTURE
--------------------------------------------------------------------------------

BeginFrame()
{
    // 1. Acquire swapchain image
    AcquireSwapchainImage();

    // 2. Reset per-frame allocators (descriptor + transient buffer)
    frameContext.Reset();

    // 3. Begin command buffer recording
    cmd.Begin();
}

--------------------------------------------------------------------------------
STEP 1: Upload Frame Constants
--------------------------------------------------------------------------------

    // Upload camera matrices to uniform buffer
    UpdateBuffer(cameraBuffer, CameraData);

    // Chunk metadata buffer contains:
    // struct ChunkData {
    //     mat4 model;
    //     vec3 boundsMin;
    //     vec3 boundsMax;
    //     uint meshIndex;
    // };
    //
    // This is persistent GPU storage buffer (updated when world changes)

--------------------------------------------------------------------------------
STEP 2: Reset Indirect Command Buffer (GPU-side)
--------------------------------------------------------------------------------

    // Indirect buffer layout:
    // struct VkDrawIndexedIndirectCommand {
    //     uint indexCount;
    //     uint instanceCount;
    //     uint firstIndex;
    //     int  vertexOffset;
    //     uint firstInstance;
    // };
    //
    // Also maintain a counter buffer:
    // uint visibleDrawCount;

    cmd.FillBuffer(visibleDrawCountBuffer, 0);

--------------------------------------------------------------------------------
STEP 3: Compute Pass – Frustum + (Optional) Occlusion Culling
--------------------------------------------------------------------------------

    cmd.BindPipeline(cullComputePipeline);
    cmd.BindStorageBuffer(0, chunkMetadataBuffer);
    cmd.BindStorageBuffer(1, indirectCommandBuffer);
    cmd.BindStorageBuffer(2, visibleDrawCountBuffer);
    cmd.BindUniformBuffer(3, cameraBuffer);

    // Each thread handles one chunk
    cmd.Dispatch(ceil(chunkCount / WORKGROUP_SIZE));

    // Compute shader:
    //
    // for each chunk:
    //     if (frustumVisible && !occluded):
    //         uint index = atomicAdd(visibleDrawCount, 1);
    //         indirectCommands[index] = BuildDrawCommand(chunk.meshIndex);

--------------------------------------------------------------------------------
STEP 4: Barrier (Compute -> Indirect Draw)
--------------------------------------------------------------------------------

    // Ensure:
    // - indirectCommandBuffer visible to draw stage
    // - visibleDrawCountBuffer visible to draw stage
    //
    cmd.InsertBarrier(
        srcStage = COMPUTE_SHADER,
        dstStage = DRAW_INDIRECT,
        srcAccess = SHADER_WRITE,
        dstAccess = INDIRECT_COMMAND_READ
    );

--------------------------------------------------------------------------------
STEP 5: Begin Main Render Pass (Tile-Friendly on Android)
--------------------------------------------------------------------------------

    cmd.BeginRenderPass(mainRenderPass);

    cmd.BindPipeline(voxelGraphicsPipeline);

    // Bind:
    // - Global scene buffer
    // - Material buffer
    // - Vertex/index buffers (shared large buffers)
    cmd.BindVertexBuffer(globalVertexBuffer);
    cmd.BindIndexBuffer(globalIndexBuffer);

--------------------------------------------------------------------------------
STEP 6: Execute Indirect Draw
--------------------------------------------------------------------------------

    cmd.DrawIndexedIndirect(
        indirectCommandBuffer,
        offset = 0,
        drawCount = maxChunkCount,     // or use count buffer variant
        stride = sizeof(VkDrawIndexedIndirectCommand)
    );

    // Preferred (if supported):
    // DrawIndexedIndirectCount(...) using visibleDrawCountBuffer

--------------------------------------------------------------------------------
STEP 7: End Render Pass
--------------------------------------------------------------------------------

    cmd.EndRenderPass();

--------------------------------------------------------------------------------
STEP 8: End Frame
--------------------------------------------------------------------------------

    cmd.End();

    Submit(cmd, frameFence);

    Present();
}

================================================================================
Important Android Notes
================================================================================

1. Keep this to ONE render pass if possible (tile GPU friendly).
2. Use LOAD_OP_DONT_CARE when safe.
3. Avoid unnecessary buffer copies.
4. Avoid large per-frame reallocations.
5. Use per-frame fences before reusing indirect buffers.
6. Keep storage buffers tightly packed to reduce bandwidth.

================================================================================
Summary
================================================================================

CPU:
    - Updates camera
    - Handles world streaming
    - Submits ONE compute dispatch
    - Submits ONE indirect draw

GPU:
    - Culls chunks
    - Builds draw command list
    - Executes only visible draws

This scales with chunk count and removes per-chunk CPU submission cost.

================================================================================
*/

#ifndef MAX_FRAMES_IN_FLIGHT
#define MAX_FRAMES_IN_FLIGHT 3
#endif

namespace OneGame::Engine::Graphics
{
    static constexpr size_t MaxColorAttachments = 4;

    struct WindowHandle
    {
#ifdef PLATFORM_WINDOWS
        void* hInstance;
        void* hwnd;
#elif defined(PLATFORM_ANDROID)
        void* nativeWindow;
#elif defined(PLATFORM_DARWIN)
        const void* metalLayer;
#endif
    };

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

    inline BufferUsage operator|(BufferUsage a, BufferUsage b)
    {
        return static_cast<BufferUsage>(
            static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
    }

    inline BufferUsage operator&(BufferUsage a, BufferUsage b)
    {
        return static_cast<BufferUsage>(
            static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
    }

    inline BufferUsage& operator|=(BufferUsage& a, BufferUsage b)
    {
        a = a | b;
        return a;
    }

    template <typename T>
    inline bool HasFlag(T value, T flag)
    {
        return (static_cast<uint32_t>(value) &
            static_cast<uint32_t>(flag)) != 0;
    }

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

    inline TextureUsage operator|(TextureUsage a, TextureUsage b)
    {
        return static_cast<TextureUsage>(
            static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
    }

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

    struct BufferDesc
    {
        uint64_t    size = 0;
        BufferUsage usage = BufferUsage::None;
        MemoryUsage memory = MemoryUsage::GPUOnly;
    };

    struct TextureDesc
    {
        uint32_t width = 1;
        uint32_t height = 1;
        uint32_t depth = 1;
        uint32_t mipLevels = 1;

		TextureFormat format = TextureFormat::BGRA8Unorm;
        TextureUsage usage = TextureUsage::Sampled;
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
        VertexAttributeFormat indexFormat;
        bool depthTest;
        bool writeDepth;
        DepthCompareOp depthCompareOp;
        bool blending;
        CullMode cullMode;
        FrontFace frontFace;

        std::vector<PushConstantRangeDesc> pushConstants;
        std::vector<GPUBindingGroupLayoutHandle> bindingGroupLayouts;
    };

    struct ComputePipelineDesc
    {
        std::vector<char> shader;
        std::vector<GPUBindingGroupLayoutHandle> bindingGroupLayouts;
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

    struct ColorAttachmentDesc
    {
        TextureFormat format;

        LoadOp  loadOp = LoadOp::Clear;
        StoreOp storeOp = StoreOp::Store;

        bool isSwapchain = false;
    };

    struct DepthAttachmentDesc
    {
        TextureFormat format;

        LoadOp  loadOp = LoadOp::Clear;
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

    struct ClearValues
    {
        std::array<std::array<float, 4>, MaxColorAttachments> colorClears;
        float depthClear = 1.0f;
        uint32_t stencilClear = 0;
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

        BindingGroupBufferDesc(GPUBufferHandle gpuBuffer) : gpuBuffer(gpuBuffer), stride(0)
        {
        }

        BindingGroupBufferDesc(GPUBufferHandle gpuBuffer, uint32_t stride) : gpuBuffer(gpuBuffer), stride(stride)
        {
        }
    };

    struct BindingGroupDesc
    {
        GPUBindingGroupLayoutHandle layout;
        std::vector<GPUTextureHandle> textures;
        std::vector<BindingGroupBufferDesc> buffers;
    };

    class ICommandList
    {
    public:
        virtual ~ICommandList() = default;

        virtual void Begin() = 0;
        virtual void End() = 0;

        // ----- Render pass -----
        virtual void BeginRenderPass(const GPURenderPassHandle renderPass, const GPUFrameBufferHandle frameBuffer, const ClearValues& clearValues) = 0;
        virtual void EndRenderPass() = 0;

        // ----- Binding -----
        virtual void BindGraphicsPipeline(GPUPipelineHandle) = 0;
        virtual void BindComputePipeline(GPUPipelineHandle) = 0;

        virtual void BindVertexBuffer(GPUBufferHandle, uint64_t offset = 0) = 0;
        virtual void BindIndexBuffer(GPUBufferHandle, uint64_t offset = 0, IndexFormat indexFormat = IndexFormat::Uint32) = 0;

        virtual void BindBindingGroup(
            GPUBindingGroupHandle,
            uint32_t setIndex,
            std::span<const uint32_t> dynamicOffsets = {}) = 0;

        virtual void PushConstants(
            ShaderStage stage,
            const void* data,
            uint32_t size) = 0;

		// ----- Buffer updates -----
		virtual void UpdateBuffer(
			GPUBufferHandle,
			uint64_t offset,
			uint64_t size,
			const void* data) = 0;

        virtual void CopyBuffer(
            GPUBufferHandle src,
            GPUBufferHandle dst,
            uint64_t size,
            uint64_t srcOffset = 0,
            uint64_t dstOffset = 0) = 0;

        virtual void CopyBufferToTexture(
            GPUBufferHandle src,
            GPUTextureHandle dst,
            uint32_t bufferOffset) = 0;

        // ----- Drawing -----
        virtual void Draw(
            uint32_t vertexCount,
            uint32_t instanceCount = 1,
            uint32_t firstVertex = 0,
            uint32_t firstInstance = 0) = 0;

        virtual void DrawIndexed(
            uint32_t indexCount,
            uint32_t instanceCount = 1,
            uint32_t firstIndex = 0,
            int32_t  vertexOffset = 0,
            uint32_t firstInstance = 0) = 0;

        virtual void DrawIndirect(
            GPUBufferHandle indirectBuffer,
            uint64_t offset,
            uint32_t drawCount,
            uint32_t stride) = 0;

        virtual void DrawIndexedIndirect(
            GPUBufferHandle indirectBuffer,
            uint64_t offset,
            uint32_t drawCount,
            uint32_t stride) = 0;

        // ----- Compute -----
        virtual void Dispatch(
            uint32_t groupX,
            uint32_t groupY,
            uint32_t groupZ) = 0;

        virtual void DispatchIndirect(
            GPUBufferHandle indirectBuffer,
            uint64_t offset) = 0;

        // ----- Barriers -----
        virtual void BufferBarrier(
            GPUBufferHandle,
            BufferUsage before,
            BufferUsage after) = 0;

        virtual void TextureBarrier(
            GPUTextureHandle,
            TextureState) = 0;
    };

    enum class BackendType {
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
        NO_COPY(IGraphicsBackend)
        virtual ~IGraphicsBackend() = default;

        virtual uint32_t MaxUniformBufferSize() const = 0;
        virtual uint32_t UniformBufferAlignment() const = 0;

        virtual uint32_t FramesInFlight() const = 0;
        virtual uint32_t CurrentFrameIndex() const = 0;

        virtual float SwapchainAspect() const = 0;
        virtual math::vec2 SwapchainExtend() const = 0;
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
        virtual void FlushStagingBufferRanges(const std::span<GPUBufferRange> ranges) = 0;

        template<BufferUsage usage>
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

        template<TextureFormat format = TextureFormat::RGBA8Unorm>
        GPUTextureHandle AllocateGPUTexture(const size_t width, const size_t height)
        {
            TextureDesc texDesc{};
            texDesc.width = width;
            texDesc.height = height;
            texDesc.format = format;
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

    std::unique_ptr<IGraphicsBackend> CreateBackend(BackendType type);
}
