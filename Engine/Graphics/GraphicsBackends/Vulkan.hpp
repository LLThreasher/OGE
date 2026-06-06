#pragma once

#include <string>
#include <memory>
#include <cstdint>
#include <stdexcept>
#include <vulkan/vulkan.h>

#include "../IGraphicsBackend.hpp"
#include "../ResourcePool.hpp"

#define VK_CHECK_RESULT(expr) do { VkResult res = (expr); if (res != VK_SUCCESS) { throw std::runtime_error("Vulkan error: " + std::to_string(res)); } } while(0)


// Forward declarations (avoid including vulkan.h here)
struct VkInstance_T;
struct VkPhysicalDevice_T;
struct VkDevice_T;
struct VkQueue_T;

struct VmaAllocator_T;
using VmaAllocator = VmaAllocator_T*;


namespace OneGame::Engine::Graphics::Vulkan
{
    static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;

    struct FrameData
    {
        VkCommandPool pool;
        std::vector<VkCommandBuffer> commandBuffers;
        uint32_t usedCount = 0;

        VkSemaphore imageAvailable;
        VkSemaphore renderFinished;
        VkFence inFlightFence;
    };

    // Forward declare internal classes
    class VulkanCommandList;

    struct VulkanBuffer;
    struct VulkanTexture;
    struct VulkanPipeline;
    struct VulkanBindingGroupLayout;
    struct VulkanBindingGroup;
    struct VulkanFence;
    struct VulkanRenderPass;
    struct VulkanFrameBuffer;
    struct VulkanRenderPassDesc;
    struct QueueIndices;

    inline VkCompareOp ToVkCompareOp(DepthCompareOp op)
    {
        switch (op)
        {
        case DepthCompareOp::Never:        return VK_COMPARE_OP_NEVER;
        case DepthCompareOp::Less:         return VK_COMPARE_OP_LESS;
        case DepthCompareOp::Equal:        return VK_COMPARE_OP_EQUAL;
        case DepthCompareOp::LessEqual:    return VK_COMPARE_OP_LESS_OR_EQUAL;
        case DepthCompareOp::Greater:      return VK_COMPARE_OP_GREATER;
        case DepthCompareOp::NotEqual:     return VK_COMPARE_OP_NOT_EQUAL;
        case DepthCompareOp::GreaterEqual: return VK_COMPARE_OP_GREATER_OR_EQUAL;
        case DepthCompareOp::Always:       return VK_COMPARE_OP_ALWAYS;
        }

        assert(false && "Unknown DepthCompareOp");
        return VK_COMPARE_OP_LESS;
    }

    inline VkCullModeFlags ToVkCullMode(CullMode mode)
    {
        switch (mode)
        {
        case CullMode::None:  return VK_CULL_MODE_NONE;
        case CullMode::Front: return VK_CULL_MODE_FRONT_BIT;
        case CullMode::Back:  return VK_CULL_MODE_BACK_BIT;
        }

        assert(false && "Unknown CullMode");
        return VK_CULL_MODE_BACK_BIT;
    }

    inline VkFrontFace ToVkFrontFace(FrontFace face)
    {
        switch (face)
        {
        case FrontFace::CW:  return VK_FRONT_FACE_CLOCKWISE;
        case FrontFace::CCW: return VK_FRONT_FACE_COUNTER_CLOCKWISE;
        }

        assert(false && "Unknown FrontFace");
        return VK_FRONT_FACE_COUNTER_CLOCKWISE;
    }

    inline void GetVkFormatAndAspect(
        TextureFormat format,
        VkFormat& outFormat,
        VkImageAspectFlags& outAspect)
    {
        switch (format)
        {
        case TextureFormat::RGBA8Unorm:
            outFormat = VK_FORMAT_R8G8B8A8_UNORM;
            outAspect = VK_IMAGE_ASPECT_COLOR_BIT;
            break;

        case TextureFormat::BGRA8Unorm:
            outFormat = VK_FORMAT_B8G8R8A8_UNORM;
            outAspect = VK_IMAGE_ASPECT_COLOR_BIT;
            break;

        case TextureFormat::Depth32Float:
            outFormat = VK_FORMAT_D32_SFLOAT;
            outAspect = VK_IMAGE_ASPECT_DEPTH_BIT;
            break;

        case TextureFormat::Depth32FloatStencil8:
			outFormat = VK_FORMAT_D32_SFLOAT_S8_UINT;
			outAspect = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
			break;

        case TextureFormat::Depth24FloatStencil8:
            outFormat = VK_FORMAT_D24_UNORM_S8_UINT;
            outAspect = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
            break;

        default:
            assert(false && "Unsupported format");
        }
    }

    inline VkImageLayout ToVkImageLayout(TextureState state)
    {
        switch (state)
        {
        case TextureState::ColorAttachment:
            return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        case TextureState::DepthAttachment:
            return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        case TextureState::ShaderRead:
            return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        case TextureState::Storage:
            return VK_IMAGE_LAYOUT_GENERAL;

        case TextureState::TransferDst:
            return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

        case TextureState::TransferSrc:
            return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

        case TextureState::Present:
            return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        default:
            return VK_IMAGE_LAYOUT_UNDEFINED;
        }
    }

	inline VkAccessFlags ToVkAccessMask(TextureState state)
	{
		switch (state)
		{
		case TextureState::ColorAttachment:
			return VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		case TextureState::DepthAttachment:
			return VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		case TextureState::ShaderRead:
			return VK_ACCESS_SHADER_READ_BIT;
		case TextureState::Storage:
			return VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
		case TextureState::TransferDst:
			return VK_ACCESS_TRANSFER_WRITE_BIT;
		case TextureState::TransferSrc:
			return VK_ACCESS_TRANSFER_READ_BIT;
		case TextureState::Present:
			return 0; // No access flags for present layout
		default:
			return 0;
		}
	}

	inline VkPipelineStageFlags ToVkPipelineStage(TextureState state)
	{
		switch (state)
		{
		case TextureState::ColorAttachment:
			return VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		case TextureState::DepthAttachment:
			return VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
		case TextureState::ShaderRead:
			return VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
		case TextureState::Storage:
			return VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
		case TextureState::TransferDst:
			return VK_PIPELINE_STAGE_TRANSFER_BIT;
		case TextureState::TransferSrc:
			return VK_PIPELINE_STAGE_TRANSFER_BIT;
		case TextureState::Present:
			return VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
		default:
			return 0;
		}
	}

    inline VkAttachmentLoadOp ToVkLoadOp(LoadOp op)
    {
        switch (op)
        {
        case LoadOp::Load:     return VK_ATTACHMENT_LOAD_OP_LOAD;
        case LoadOp::Clear:    return VK_ATTACHMENT_LOAD_OP_CLEAR;
        case LoadOp::DontCare: return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        }
        return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    }

	inline VkAttachmentStoreOp ToVkStoreOp(StoreOp op)
	{
		switch (op)
		{
		case StoreOp::Store:   return VK_ATTACHMENT_STORE_OP_STORE;
		case StoreOp::DontCare:return VK_ATTACHMENT_STORE_OP_DONT_CARE;
		}
		return VK_ATTACHMENT_STORE_OP_DONT_CARE;
	}
    struct FormatInfo
    {
        VkFormat vkFormat;
        uint32_t size;
    };

    inline FormatInfo GetFormatInfo(VertexAttributeFormat format)
    {
        switch (format)
        {
        case VertexAttributeFormat::Float32:   return { VK_FORMAT_R32_SFLOAT, 4 };
        case VertexAttributeFormat::Float32x2: return { VK_FORMAT_R32G32_SFLOAT, 8 };
        case VertexAttributeFormat::Float32x3: return { VK_FORMAT_R32G32B32_SFLOAT, 12 };
        case VertexAttributeFormat::Float32x4: return { VK_FORMAT_R32G32B32A32_SFLOAT, 16 };
        case VertexAttributeFormat::Uint32:    return { VK_FORMAT_R32_UINT, 4 };
        case VertexAttributeFormat::Uint32x2:  return { VK_FORMAT_R32G32_UINT, 8 };
        case VertexAttributeFormat::Uint32x3:  return { VK_FORMAT_R32G32B32_UINT, 12 };
        case VertexAttributeFormat::Uint32x4:  return { VK_FORMAT_R32G32B32A32_UINT, 16 };
        }

        assert(false);
        return { VK_FORMAT_UNDEFINED, 0 };
    }

    inline VkFormat ToVkFormat(TextureFormat format)
    {
        switch (format)
        {
        case TextureFormat::RGBA8Unorm:  return VK_FORMAT_R8G8B8A8_UNORM;
        case TextureFormat::RGBA8Srgb:   return VK_FORMAT_R8G8B8A8_SRGB;
        case TextureFormat::BGRA8Unorm:  return VK_FORMAT_B8G8R8A8_UNORM;
        case TextureFormat::BGRA8Srgb:   return VK_FORMAT_B8G8R8A8_SRGB;
        case TextureFormat::R8Unorm:     return VK_FORMAT_R8_UNORM;
        case TextureFormat::R16Float:    return VK_FORMAT_R16_SFLOAT;
        case TextureFormat::RG16Float:   return VK_FORMAT_R16G16_SFLOAT;
        case TextureFormat::RGBA16Float: return VK_FORMAT_R16G16B16A16_SFLOAT;
        case TextureFormat::R32Float:    return VK_FORMAT_R32_SFLOAT;
        case TextureFormat::RG32Float:   return VK_FORMAT_R32G32_SFLOAT;
        case TextureFormat::RGBA32Float: return VK_FORMAT_R32G32B32A32_SFLOAT;
        case TextureFormat::Depth24FloatStencil8:
            return VK_FORMAT_D24_UNORM_S8_UINT;
        case TextureFormat::Depth32Float:
            return VK_FORMAT_D32_SFLOAT;
        default:
            assert(false && "Unknown TextureFormat");
            return VK_FORMAT_UNDEFINED;
        }
    }

	inline VkShaderStageFlags ToVkShaderStage(ShaderStage stage)
	{
        VkShaderStageFlags flags = 0;

		if (HasFlag(stage, ShaderStage::Vertex))
			flags |= VK_SHADER_STAGE_VERTEX_BIT;
		if (HasFlag(stage, ShaderStage::Fragment))
			flags |= VK_SHADER_STAGE_FRAGMENT_BIT;
		if (HasFlag(stage, ShaderStage::Compute))
			flags |= VK_SHADER_STAGE_COMPUTE_BIT;

		return flags;
	}

    inline VkAccessFlags ConvertAccessMask(BufferUsage usage)
    {
        VkAccessFlags flags = 0;

        if (HasFlag(usage, BufferUsage::Vertex))
            flags |= VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;

        if (HasFlag(usage, BufferUsage::Index))
            flags |= VK_ACCESS_INDEX_READ_BIT;

        if (HasFlag(usage, BufferUsage::Uniform))
            flags |= VK_ACCESS_UNIFORM_READ_BIT;

        if (HasFlag(usage, BufferUsage::Storage))
            flags |= VK_ACCESS_SHADER_READ_BIT |
            VK_ACCESS_SHADER_WRITE_BIT;

        if (HasFlag(usage, BufferUsage::Indirect))
            flags |= VK_ACCESS_INDIRECT_COMMAND_READ_BIT;

        if (HasFlag(usage, BufferUsage::TransferSrc))
            flags |= VK_ACCESS_TRANSFER_READ_BIT;

        if (HasFlag(usage, BufferUsage::TransferDst))
            flags |= VK_ACCESS_TRANSFER_WRITE_BIT;

        return flags;
    }

	inline VkAccessFlags ConvertAccessMask(TextureUsage usage)
	{
		VkAccessFlags flags = 0;
		if (HasFlag(usage, TextureUsage::Sampled))
			flags |= VK_ACCESS_SHADER_READ_BIT;
		if (HasFlag(usage, TextureUsage::Storage))
			flags |= VK_ACCESS_SHADER_READ_BIT |
			VK_ACCESS_SHADER_WRITE_BIT;
		if (HasFlag(usage, TextureUsage::ColorAttachment))
			flags |= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		if (HasFlag(usage, TextureUsage::DepthAttachment))
			flags |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
			VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		if (HasFlag(usage, TextureUsage::TransferSrc))
			flags |= VK_ACCESS_TRANSFER_READ_BIT;
		if (HasFlag(usage, TextureUsage::TransferDst))
			flags |= VK_ACCESS_TRANSFER_WRITE_BIT;
		return flags;
	}

    inline VkPipelineStageFlags ConvertPipelineStage(BufferUsage usage)
    {
        VkPipelineStageFlags flags = 0;

        if (HasFlag(usage, BufferUsage::Vertex) ||
            HasFlag(usage, BufferUsage::Index))
        {
            flags |= VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
        }

        if (HasFlag(usage, BufferUsage::Uniform) ||
            HasFlag(usage, BufferUsage::Storage))
        {
            flags |= VK_PIPELINE_STAGE_VERTEX_SHADER_BIT |
                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        }

        if (HasFlag(usage, BufferUsage::Indirect))
        {
            flags |= VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT;
        }

        if (HasFlag(usage, BufferUsage::TransferSrc) ||
            HasFlag(usage, BufferUsage::TransferDst))
        {
            flags |= VK_PIPELINE_STAGE_TRANSFER_BIT;
        }

        if (flags == 0)
            flags = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;

        return flags;
    }

    class VulkanCommandBuffer;

    struct VulkanDevice
    {
        VkDevice            device          = VK_NULL_HANDLE;
        VkInstance          instance        = VK_NULL_HANDLE;
        VkSurfaceKHR        surface         = VK_NULL_HANDLE;
		VkPhysicalDevice    physicalDevice  = VK_NULL_HANDLE;
        VmaAllocator        m_allocator     = nullptr;

        VkSurfaceFormatKHR  surfaceFormat;
		VkFormat		    depthFormat;
		VkPresentModeKHR    presentMode;

        VkQueue m_graphicsQueue;
        VkQueue m_computeQueue;
        VkQueue m_transferQueue;
        VkQueue m_presentQueue;
    };

    struct VulkanSwapchain
    {
        VkSwapchainKHR swapchain = VK_NULL_HANDLE;
        VkExtent2D extent;
        VkExtent2D nextExtent;
        std::vector<GPUTextureHandle> colorTextures;
        std::vector<GPUTextureHandle> depthTextures;
		GPURenderPassHandle renderPass;
		std::vector<GPUFrameBufferHandle> framebuffers;
    };

    class VulkanBackend final : public IGraphicsBackend
    {
        friend class VulkanCommandBuffer;

    public:
        VulkanBackend();
        ~VulkanBackend() override;

        uint32_t MaxUniformBufferSize() const override;
        uint32_t UniformBufferAlignment() const override;

        uint32_t FramesInFlight() const override;
        uint32_t CurrentFrameIndex() const override;

        void Initialize(const BackendDesc&) override;
        void Resize(uint32_t width, uint32_t height) override;
        void Shutdown() override;

        void BeginFrame() override;
        void EndFrame() override;

        ICommandList* CreateCommandList(QueueType) override;
        void Submit(ICommandList*) override;

        // ----- Buffers -----
        GPUBufferHandle CreateBuffer(const BufferDesc&) override;
        void DestroyBuffer(GPUBufferHandle) override;

        // ----- Textures -----
        GPUTextureHandle CreateTexture(const TextureDesc&) override;
        void DestroyTexture(GPUTextureHandle) override;

        // ----- Pipelines -----
        GPUPipelineHandle CreateGraphicsPipeline(const GraphicsPipelineDesc&) override;
        GPUPipelineHandle CreateComputePipeline(const ComputePipelineDesc&) override;
        void DestroyPipeline(GPUPipelineHandle) override;

        // ----- Binding groups -----
        GPUBindingGroupLayoutHandle CreateBindingGroupLayout(const BindingGroupLayoutDesc&) override;
        void DestroyBindingGroupLayout(GPUBindingGroupLayoutHandle) override;

        GPUBindingGroupHandle CreateBindingGroup(const BindingGroupDesc&) override;
        void DestroyBindingGroup(GPUBindingGroupHandle) override;

        // ----- Sync -----
        GPUFenceHandle CreateFence(bool signaled = false) override;
        void WaitForFence(GPUFenceHandle) override;
        bool IsFenceSignaled(GPUFenceHandle) override;
        void ResetFence(GPUFenceHandle) override;

        GPURenderPassHandle CreateRenderPass(const RenderPassDesc&) override;
        void DestroyRenderPass(GPURenderPassHandle) override;

        GPUFrameBufferHandle CreateFrameBuffer(GPURenderPassHandle passHandle, const FrameBufferDesc&) override;
        void DestroyFrameBuffer(GPUFrameBufferHandle) override;

		GPURenderPassHandle GetCurrentRenderPass() override;
        GPUFrameBufferHandle GetCurrentFrameBuffer() override;

    private:
        VulkanDevice                m_device;
        VulkanSwapchain             m_swapchain;
        uint32_t			        m_frameIndex = 0;
        std::vector<FrameData>      m_frames;
        uint32_t			        m_imageIndex = 0;
        std::vector<VkFence>        m_imagesInFlight;
        uint32_t                    m_imagesAvailableSlot = 0;
        std::vector<VkSemaphore>    m_imagesFinishRender;

        VkDescriptorPool    m_descriptorPool;
#ifdef _DEBUG
        VkDebugUtilsMessengerEXT m_debugMessenger;
#endif
        ResourcePool<GPUBuffer, VulkanBuffer> m_buffers;
        ResourcePool<GPUTexture, VulkanTexture> m_textures;
        ResourcePool<GPUPipeline, VulkanPipeline> m_pipelines;
        ResourcePool<GPUBindingGroupLayout, VulkanBindingGroupLayout> m_bindingGroupLayouts;
        ResourcePool<GPUBindingGroup, VulkanBindingGroup> m_bindingGroups;
        ResourcePool<GPUFence, VulkanFence> m_fences;
        ResourcePool<GPURenderPass, VulkanRenderPass> m_renderPasses;
        ResourcePool<GPUFrameBuffer, VulkanFrameBuffer> m_frameBuffers;

        VulkanRenderPass CreateRenderPassInternal(VulkanRenderPassDesc&);
        void CreateSyncObjects(QueueIndices&, int);
        void RecreateSwapchain(int&);
        void DestroySwapchain(VkSwapchainKHR);
        VkShaderModule CreateShaderModule(const std::vector<uint8_t>& code);
        void CreateTextureInternal(
            uint32_t width,
            uint32_t height,
            uint32_t depth,
            uint32_t mipLevels,
            VkImageUsageFlags vkUsage,
            VkFormat vkFormat,
            VkImageAspectFlags aspectMask,
            VulkanTexture& result);
		void DestroyTextureInternal(VulkanTexture& texture);
        void CreateSwapchainRenderPass();
        void CreateSwapchainFrameBuffers();
    };

}
