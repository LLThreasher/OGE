#include "metal.hpp"

#include <Metal/Metal.hpp>
#include <QuartzCore/CAMetalLayer.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "binding_group.hpp"
#include "buffer.hpp"
#include "command_buffer.hpp"
#include "fence.hpp"
#include "oge/graphics/backend.hpp"
#include "oge/graphics/configs.hpp"
#include "pipeline.hpp"
#include "texture.hpp"

#define LOGGER_NAME "Metal"
#include "oge/log.hpp"
#include "oge/math.hpp"
#include "oge/platform/stacktrace.hpp"

namespace oge::graphics::metal
{

static const char* FeatureSetGPUFamilyToString(MTL::GPUFamily family)
{
    switch (family)
    {
        case MTL::GPUFamilyApple1:
            return "Apple1";
        case MTL::GPUFamilyApple2:
            return "Apple2";
        case MTL::GPUFamilyApple3:
            return "Apple3";
        case MTL::GPUFamilyApple4:
            return "Apple4";
        case MTL::GPUFamilyApple5:
            return "Apple5";
        case MTL::GPUFamilyApple6:
            return "Apple6";
        case MTL::GPUFamilyApple7:
            return "Apple7";
        case MTL::GPUFamilyApple8:
            return "Apple8";
        case MTL::GPUFamilyMac1:
            return "Mac1";
        case MTL::GPUFamilyMac2:
            return "Mac2";
        case MTL::GPUFamilyCommon1:
            return "Common1";
        case MTL::GPUFamilyCommon2:
            return "Common2";
        case MTL::GPUFamilyCommon3:
            return "Common3";
        default:
            return "Unknown";
    }
}

static bool SupportsFamily(MTL::Device* device, MTL::GPUFamily family)
{
    if (device == nullptr) return false;
    return device->supportsFamily(family);
}

static bool IsAppleSiliconGPU(MTL::Device* device)
{
    if (device == nullptr) return false;

    return SupportsFamily(device, MTL::GPUFamilyApple1) ||
           SupportsFamily(device, MTL::GPUFamilyApple2) ||
           SupportsFamily(device, MTL::GPUFamilyApple3) ||
           SupportsFamily(device, MTL::GPUFamilyApple4) ||
           SupportsFamily(device, MTL::GPUFamilyApple5) ||
           SupportsFamily(device, MTL::GPUFamilyApple6) ||
           SupportsFamily(device, MTL::GPUFamilyApple7) ||
           SupportsFamily(device, MTL::GPUFamilyApple8);
}

static bool IsMacGPU(MTL::Device* device)
{
    if (device == nullptr) return false;

    return SupportsFamily(device, MTL::GPUFamilyMac1) ||
           SupportsFamily(device, MTL::GPUFamilyMac2);
}

static int RateDevice(MTL::Device* device)
{
    if (device == nullptr) return 0;

    int score = 0;

    if (!device->isLowPower())
    {
        score += 1000;
    }

    if (device->isRemovable())
    {
        score += 500;
    }

    if (IsAppleSiliconGPU(device))
    {
        score += 300;
    }

    if (IsMacGPU(device))
    {
        score += 200;
    }

    score += static_cast<int>(device->maxThreadsPerThreadgroup().width);
    score += static_cast<int>(device->maxThreadsPerThreadgroup().height);
    score += static_cast<int>(device->maxThreadsPerThreadgroup().depth);

    return score;
}

static void PrintMetalDeviceInfo(MTL::Device* device)
{
    if (device == nullptr)
    {
        LOG_WARN("Metal device is null");
        return;
    }

    LOG_INFO("========== GPU ==========");

    if (device->name() != nullptr)
    {
        LOG_INFO("Name: {}", std::string_view(device->name()->utf8String()));
    }
    else
    {
        LOG_INFO("Name: <unknown>");
    }

    LOG_INFO("Low Power: {}", device->isLowPower() ? "Yes" : "No");
    LOG_INFO("Headless: {}", device->isHeadless() ? "Yes" : "No");
    LOG_INFO("Removable: {}", device->isRemovable() ? "Yes" : "No");

    LOG_INFO("Recommended Max Working Set Size: {} MB",
             static_cast<uint64_t>(device->recommendedMaxWorkingSetSize() /
                                   (1024ull * 1024ull)));

    const auto maxThreads = device->maxThreadsPerThreadgroup();

    LOG_INFO("Max Threads Per Threadgroup: {} x {} x {}", maxThreads.width,
             maxThreads.height, maxThreads.depth);

    LOG_INFO("Current Allocated Size: {} MB",
             static_cast<uint64_t>(device->currentAllocatedSize() /
                                   (1024ull * 1024ull)));

    LOG_INFO("Location Number: {}", device->locationNumber());
    LOG_INFO("Registry ID: {}", device->registryID());

    LOG_INFO("---- GPU Families ----");

    const MTL::GPUFamily families[] = {
        MTL::GPUFamilyApple1,  MTL::GPUFamilyApple2,
        MTL::GPUFamilyApple3,  MTL::GPUFamilyApple4,
        MTL::GPUFamilyApple5,  MTL::GPUFamilyApple6,
        MTL::GPUFamilyApple7,  MTL::GPUFamilyApple8,
        MTL::GPUFamilyMac1,    MTL::GPUFamilyMac2,
        MTL::GPUFamilyCommon1, MTL::GPUFamilyCommon2,
        MTL::GPUFamilyCommon3,
    };

    for (auto family : families)
    {
        if (device->supportsFamily(family))
        {
            LOG_INFO("Supports {}", FeatureSetGPUFamilyToString(family));
        }
    }

    LOG_INFO("========================");
}

static void PrintAllMetalDeviceInfo()
{
    NS::Array* devices = MTL::CopyAllDevices();

    if (devices == nullptr || devices->count() == 0)
    {
        throw std::runtime_error("Failed to find GPUs with Metal support!");
    }

    for (NS::UInteger i = 0; i < devices->count(); ++i)
    {
        auto* device = static_cast<MTL::Device*>(devices->object(i));
        PrintMetalDeviceInfo(device);
    }

    devices->release();
}

static NS::SharedPtr<MTL::Device> SelectMetalDevice()
{
    NS::Array* devices = MTL::CopyAllDevices();

    if (devices == nullptr || devices->count() == 0)
    {
        throw std::runtime_error("Failed to find GPUs with Metal support!");
    }

    MTL::Device* bestDevice = nullptr;
    int bestScore = -1;

    for (NS::UInteger i = 0; i < devices->count(); ++i)
    {
        auto* device = static_cast<MTL::Device*>(devices->object(i));
        const int score = RateDevice(device);

        if (score > bestScore)
        {
            bestScore = score;
            bestDevice = device;
        }
    }

    NS::SharedPtr<MTL::Device> result;

    if (bestDevice != nullptr)
    {
        result = NS::TransferPtr(bestDevice->retain());
    }

    devices->release();

    if (result == nullptr)
    {
        throw std::runtime_error("Failed to select suitable Metal device!");
    }

    return result;
}

static MTL::PixelFormat ChooseSwapchainPixelFormat()
{
    // Closest common equivalent to Vulkan VK_FORMAT_B8G8R8A8_SRGB.
    return MTL::PixelFormatBGRA8Unorm_sRGB;
}

static MTL::PixelFormat ChooseDepthPixelFormat()
{
    // Closest common equivalent to your Vulkan VK_FORMAT_D16_UNORM choice.
    return MTL::PixelFormatDepth16Unorm;
}

MetalBackend::MetalBackend()
{
}

MetalBackend::~MetalBackend()
{
}

uint32_t MetalBackend::MaxUniformBufferSize() const
{
    // Metal does not expose an exact Vulkan-style maxUniformBufferRange.
    // For `setVertexBytes` / `setFragmentBytes`, Metal has a much smaller
    // inline constant-data limit, but for MTLBuffer-backed uniform/storage data
    // this engine-level limit can be chosen conservatively.
    //
    // 64 KiB is a safe practical uniform-buffer range comparable to common
    // D3D/Vulkan constant/uniform buffer expectations.
    return 64u * 1024u;
}

uint32_t MetalBackend::UniformBufferAlignment() const
{
    // Metal buffer offsets for constant/address-space buffers should generally
    // be 256-byte aligned for portable dynamic uniform offsets.
    return 256u;
}

uint32_t MetalBackend::FramesInFlight() const
{
    return static_cast<uint32_t>(m_frames.size());
}

uint32_t MetalBackend::CurrentFrameIndex() const
{
    return m_frameIndex;
}

float MetalBackend::SwapchainAspect() const
{
    const auto extent = SwapchainExtent();

    if (extent.y == 0)
    {
        return 0.0f;
    }

    return static_cast<float>(extent.x) / static_cast<float>(extent.y);
}

U16Point2 MetalBackend::SwapchainExtent() const
{
    return m_swapchain.extent;
}

math::Orientation MetalBackend::SwapchainPretransform() const
{
    return m_swapchain.currentTransform;
}

bool MetalBackend::SwapchainRecreated() const
{
    return m_swapchain.wasRecreated || m_swapchain.isDirty;
}

GPUInfo MetalBackend::GetGPUInfo() const
{
    GPUInfo result{};

    if (m_device.device == nullptr)
    {
        return result;
    }

    MTL::Device* device = m_device.device.get();

    if (device->name() != nullptr)
    {
        result.name = device->name()->utf8String();
    }

    // Metal does not expose Vulkan-style memory heap arrays.
    // Treat it as one logical heap for your generic API.
    result.heapCount = 1;

    return result;
}

GPUMemoryUsage MetalBackend::GetGPUMemoryUsage() const
{
    GPUMemoryUsage result{};

    if (m_device.device == nullptr)
    {
        return result;
    }

    MTL::Device* device = m_device.device.get();

    result.heapUsage[0] = device->currentAllocatedSize();
    result.heapBudget[0] = device->recommendedMaxWorkingSetSize();

    return result;
}

MTL::PixelFormat MetalBackend::ToMetalPixelFormat(TextureFormat format) const
{
    switch (format)
    {
        case TextureFormat::RGBA8Unorm:
            return MTL::PixelFormatRGBA8Unorm;

        case TextureFormat::RGBA8Srgb:
            return MTL::PixelFormatRGBA8Unorm_sRGB;

        case TextureFormat::BGRA8Unorm:
            return MTL::PixelFormatBGRA8Unorm;

        case TextureFormat::BGRA8Srgb:
            return MTL::PixelFormatBGRA8Unorm_sRGB;

        case TextureFormat::R8Unorm:
            return MTL::PixelFormatR8Unorm;

        case TextureFormat::R16Float:
            return MTL::PixelFormatR16Float;

        case TextureFormat::RG16Float:
            return MTL::PixelFormatRG16Float;

        case TextureFormat::RGBA16Float:
            return MTL::PixelFormatRGBA16Float;

        case TextureFormat::R32Float:
            return MTL::PixelFormatR32Float;

        case TextureFormat::RG32Float:
            return MTL::PixelFormatRG32Float;

        case TextureFormat::RGBA32Float:
            return MTL::PixelFormatRGBA32Float;

        case TextureFormat::Depth16Unorm:
            return MTL::PixelFormatDepth16Unorm;

        case TextureFormat::Depth32Float:
            return MTL::PixelFormatDepth32Float;

        case TextureFormat::Depth24Stencil8:
            return MTL::PixelFormatDepth24Unorm_Stencil8;

        case TextureFormat::Depth32FloatStencil8:
            return MTL::PixelFormatDepth32Float_Stencil8;

        default:
            LOG_ERROR("Unsupported TextureFormat {}", static_cast<uint32_t>(format));
            throw std::runtime_error("Unsupported TextureFormat for Metal");
    }
}

MTL::TextureUsage MetalBackend::ToMetalTextureUsage(
    TextureUsageFlags usage) const
{
    MTL::TextureUsage result = MTL::TextureUsageUnknown;

    if ((usage & TextureUsage::Sampled) == TextureUsage::Sampled)
    {
        result |= MTL::TextureUsageShaderRead;
    }

    if ((usage & TextureUsage::Storage) == TextureUsage::Storage)
    {
        result |= MTL::TextureUsageShaderWrite;
    }

    if ((usage & TextureUsage::RenderAttachment) ==
        TextureUsage::RenderAttachment)
    {
        result |= MTL::TextureUsageRenderTarget;
    }

    if ((usage & TextureUsage::TransferSrc) == TextureUsage::TransferSrc)
    {
        result |= MTL::TextureUsageShaderRead;
    }

    if ((usage & TextureUsage::TransferDst) == TextureUsage::TransferDst)
    {
        result |= MTL::TextureUsageShaderWrite;
    }

    if (result == MTL::TextureUsageUnknown)
    {
        result = MTL::TextureUsageShaderRead;
    }

    return result;
}

MTL::ResourceOptions MetalBackend::ToMetalResourceOptions(
    MemoryUsage usage) const
{
    MTL::ResourceOptions options = MTL::ResourceCPUCacheModeDefaultCache;

    switch (usage)
    {
        case MemoryUsage::CPUOnly:
            options |= MTL::ResourceStorageModeShared;
            break;

        case MemoryUsage::CPUToGPU:
            options |= MTL::ResourceStorageModeShared;
            break;

        case MemoryUsage::GPUToCPU:
            options |= MTL::ResourceStorageModeShared;
            break;

        case MemoryUsage::GPUOnly:
            options |= MTL::ResourceStorageModePrivate;
            break;

        default:
            options |= MTL::ResourceStorageModePrivate;
            break;
    }

    return options;
}

MTL::StorageMode MetalBackend::ToMetalStorageMode(MemoryUsage usage) const
{
    switch (usage)
    {
        case MemoryUsage::CPUOnly:
        case MemoryUsage::CPUToGPU:
        case MemoryUsage::GPUToCPU:
            return MTL::StorageModeShared;

        case MemoryUsage::GPUOnly:
            return MTL::StorageModePrivate;

        default:
            return MTL::StorageModePrivate;
    }
}

MTL::CPUCacheMode MetalBackend::ToMetalCPUCacheMode(MemoryUsage usage) const
{
    switch (usage)
    {
        case MemoryUsage::CPUOnly:
        case MemoryUsage::CPUToGPU:
        case MemoryUsage::GPUToCPU:
            return MTL::CPUCacheModeDefaultCache;

        case MemoryUsage::GPUOnly:
            return MTL::CPUCacheModeDefaultCache;

        default:
            return MTL::CPUCacheModeDefaultCache;
    }
}

MTL::LoadAction MetalBackend::ToMetalLoadAction(LoadOp op) const
{
    switch (op)
    {
        case LoadOp::Load:
            return MTL::LoadActionLoad;

        case LoadOp::Clear:
            return MTL::LoadActionClear;

        case LoadOp::DontCare:
            return MTL::LoadActionDontCare;

        default:
            return MTL::LoadActionDontCare;
    }
}

MTL::StoreAction MetalBackend::ToMetalStoreAction(StoreOp op) const
{
    switch (op)
    {
        case StoreOp::Store:
            return MTL::StoreActionStore;

        case StoreOp::DontCare:
            return MTL::StoreActionDontCare;

        default:
            return MTL::StoreActionDontCare;
    }
}

MTL::PrimitiveType MetalBackend::ToMetalPrimitiveType(
    PrimitiveTopology topology) const
{
    switch (topology)
    {
        case PrimitiveTopology::PointList:
            return MTL::PrimitiveTypePoint;

        case PrimitiveTopology::LineList:
            return MTL::PrimitiveTypeLine;

        case PrimitiveTopology::LineStrip:
            return MTL::PrimitiveTypeLineStrip;

        case PrimitiveTopology::TriangleList:
            return MTL::PrimitiveTypeTriangle;

        case PrimitiveTopology::TriangleStrip:
            return MTL::PrimitiveTypeTriangleStrip;

        default:
            return MTL::PrimitiveTypeTriangle;
    }
}

MTL::IndexType MetalBackend::ToMetalIndexType(IndexFormat format) const
{
    switch (format)
    {
        case IndexFormat::UInt16:
            return MTL::IndexTypeUInt16;

        case IndexFormat::UInt32:
            return MTL::IndexTypeUInt32;

        default:
            return MTL::IndexTypeUInt32;
    }
}

MTL::CompareFunction MetalBackend::ToMetalCompareFunction(
    CompareFunction function) const
{
    switch (function)
    {
        case CompareFunction::Never:
            return MTL::CompareFunctionNever;

        case CompareFunction::Less:
            return MTL::CompareFunctionLess;

        case CompareFunction::Equal:
            return MTL::CompareFunctionEqual;

        case CompareFunction::LessEqual:
            return MTL::CompareFunctionLessEqual;

        case CompareFunction::Greater:
            return MTL::CompareFunctionGreater;

        case CompareFunction::NotEqual:
            return MTL::CompareFunctionNotEqual;

        case CompareFunction::GreaterEqual:
            return MTL::CompareFunctionGreaterEqual;

        case CompareFunction::Always:
            return MTL::CompareFunctionAlways;

        default:
            return MTL::CompareFunctionAlways;
    }
}

MTL::CullMode MetalBackend::ToMetalCullMode(CullMode mode) const
{
    switch (mode)
    {
        case CullMode::None:
            return MTL::CullModeNone;

        case CullMode::Front:
            return MTL::CullModeFront;

        case CullMode::Back:
            return MTL::CullModeBack;

        default:
            return MTL::CullModeNone;
    }
}

MTL::Winding MetalBackend::ToMetalWinding(FrontFace face) const
{
    switch (face)
    {
        case FrontFace::Clockwise:
            return MTL::WindingClockwise;

        case FrontFace::CounterClockwise:
            return MTL::WindingCounterClockwise;

        default:
            return MTL::WindingCounterClockwise;
    }
}

bool MetalBackend::CreateDevice()
{
    m_device.device = SelectMetalDevice();

    if (m_device.device == nullptr)
    {
        return false;
    }

    m_device.swapchainFormat = ChooseSwapchainPixelFormat();
    m_device.depthFormat = ChooseDepthPixelFormat();

    PrintAllMetalDeviceInfo();
    PrintMetalDeviceInfo(m_device.device.get());

    return true;
}

void MetalBackend::CreateCommandQueues()
{
    if (m_device.device == nullptr)
    {
        throw std::runtime_error("Cannot create Metal command queues without device");
    }

    m_device.graphicsQueue =
        NS::TransferPtr(m_device.device->newCommandQueue());
    m_device.computeQueue =
        NS::TransferPtr(m_device.device->newCommandQueue());
    m_device.transferQueue =
        NS::TransferPtr(m_device.device->newCommandQueue());

    if (m_device.graphicsQueue == nullptr)
    {
        throw std::runtime_error("Failed to create Metal graphics command queue");
    }

    if (m_device.computeQueue == nullptr)
    {
        throw std::runtime_error("Failed to create Metal compute command queue");
    }

    if (m_device.transferQueue == nullptr)
    {
        throw std::runtime_error("Failed to create Metal transfer command queue");
    }
}

static CA::MetalLayer* RetainMetalLayer(WindowHandle* handle)
{
    if (handle == nullptr)
    {
        throw std::runtime_error("Metal RetainMetalLayer failed: WindowHandle is null");
    }

    if (handle->metalLayer == nullptr)
    {
        throw std::runtime_error("Metal RetainMetalLayer failed: WindowHandle::metalLayer is null");
    }

    auto* layer = reinterpret_cast<CA::MetalLayer*>(
        const_cast<void*>(handle->metalLayer));

    layer->retain();

    return layer;
}

static void ReleaseMetalLayer(CA::MetalLayer*& layer)
{
    if (layer != nullptr)
    {
        layer->release();
        layer = nullptr;
    }
}

void MetalBackend::CreateSwapchain(WindowHandle* handle)
{
    DestroySwapchain();

    m_swapchain.layer = RetainMetalLayer(handle);
    m_device.layer = m_swapchain.layer;

    m_swapchain.layer->setDevice(m_device.device.get());
    m_swapchain.layer->setPixelFormat(m_device.swapchainFormat);
    m_swapchain.layer->setFramebufferOnly(true);

    const CGSize drawableSize = m_swapchain.layer->drawableSize();

    m_swapchain.extent = {
        static_cast<uint16_t>(drawableSize.width),
        static_cast<uint16_t>(drawableSize.height),
    };

    m_swapchain.nextExtent = m_swapchain.extent;
    m_swapchain.currentTransform = math::Orientation::IDENTITY;
    m_swapchain.isDirty = true;
    m_swapchain.wasRecreated = true;
}

void MetalBackend::DestroySwapchain()
{
    if (m_swapchain.currentDrawable != nullptr)
    {
        m_swapchain.currentDrawable->release();
        m_swapchain.currentDrawable = nullptr;
    }

    if (m_swapchain.currentColorTexture.IsValid())
    {
        MetalTexture* tex = m_textures.Get(m_swapchain.currentColorTexture);
        if (tex != nullptr)
        {
            tex->texture = nullptr;
        }

        m_textures.Destroy(m_swapchain.currentColorTexture);
        m_swapchain.currentColorTexture = {};
    }

    if (m_swapchain.currentDepthTexture.IsValid())
    {
        DestroyTexture(m_swapchain.currentDepthTexture);
        m_swapchain.currentDepthTexture = {};
    }

    ReleaseMetalLayer(m_swapchain.layer);

    m_device.layer = nullptr;

    m_swapchain.extent = {};
    m_swapchain.nextExtent = {};
    m_swapchain.isDirty = false;
    m_swapchain.wasRecreated = false;
}

void MetalBackend::RecreateSurface(WindowHandle* handle)
{
    WaitDeviceIdle();

    DestroySwapchain();
    CreateSwapchain(handle);
}

void MetalBackend::Resize(uint32_t width, uint32_t height)
{
    m_swapchain.nextExtent = {
        static_cast<uint16_t>(width),
        static_cast<uint16_t>(height),
    };

    if (m_swapchain.layer != nullptr)
    {
        CGSize size;
        size.width = static_cast<CGFloat>(width);
        size.height = static_cast<CGFloat>(height);

        m_swapchain.layer->setDrawableSize(size);
    }

    m_swapchain.isDirty = true;
}

void MetalBackend::Initialize(const BackendDesc& desc)
{
#ifndef PLATFORM_DARWIN
    (void)desc;
    throw std::runtime_error("Metal backend is only supported on Darwin platforms");
#else
    if (desc.window == nullptr)
    {
        throw std::runtime_error("MetalBackend::Initialize failed: BackendDesc::window is null");
    }

    if (desc.window->metalLayer == nullptr)
    {
        throw std::runtime_error("MetalBackend::Initialize failed: WindowHandle::metalLayer is null");
    }

    LOG_INFO("Initializing Metal backend");

    m_frameIndex = 0;

    // Store frame pacing preference if your backend has such a field.
    // Otherwise remove this line.
    m_frameTimePreference = desc.frameTime;

    if (!CreateDevice())
    {
        throw std::runtime_error("MetalBackend::Initialize failed: failed to create Metal device");
    }

    CreateCommandQueues();

    CreateSwapchain(desc.window);

    // If you have explicit per-frame resources, initialize them here.
    //
    // Example:
    //
    // constexpr uint32_t framesInFlight = 2;
    // m_frames.resize(framesInFlight);
    //
    //
    // If your MetalFrame struct only contains NS::SharedPtr/engine handles,
    // resize alone may be enough.
    for (uint32_t i = 0; i < m_frames.size(); ++i)
    {
        m_frames[i].inFlightSemaphore = dispatch_semaphore_create(1);
    }
    if (m_frames.empty())
    {
        uint32_t drawableCount = std::clamp(MAX_FRAMES_IN_FLIGHT, 2u, 3u);
        m_frames.resize(drawableCount);
    }

    m_swapchain.wasRecreated = true;
    m_swapchain.isDirty = false;

    LOG_INFO("Metal backend initialized");
#endif
}

void MetalBackend::RecreateSurface(WindowHandle* handle)
{
#ifndef PLATFORM_DARWIN
    (void)handle;
    throw std::runtime_error("Metal backend is only supported on Darwin platforms");
#else
    if (handle == nullptr)
    {
        throw std::runtime_error("MetalBackend::RecreateSurface failed: WindowHandle is null");
    }

    if (handle->metalLayer == nullptr)
    {
        throw std::runtime_error("MetalBackend::RecreateSurface failed: WindowHandle::metalLayer is null");
    }

    LOG_INFO("Recreating Metal surface");

    WaitDeviceIdle();

    DestroySwapchain();
    CreateSwapchain(handle);

    m_swapchain.wasRecreated = true;
    m_swapchain.isDirty = false;

    LOG_INFO("Metal surface recreated");
#endif
}

void MetalBackend::WaitDeviceIdle()
{
    if (m_device.graphicsQueue == nullptr)
    {
        return;
    }

    MTL::CommandBuffer* commandBuffer =
        m_device.graphicsQueue->commandBuffer();

    if (commandBuffer == nullptr)
    {
        return;
    }

    commandBuffer->commit();
    commandBuffer->waitUntilCompleted();

    commandBuffer->release();
}

void MetalBackend::Shutdown()
{
    LOG_INFO("Shutting down Metal backend");

    WaitDeviceIdle();

    DestroySwapchain();

    // Destroy per-frame resources if your frame struct owns any raw Objective-C
    // objects or dispatch semaphores.
    //
    // Example:
    //
    // for (auto& frame : m_frames)
    // {
    //     if (frame.inFlightSemaphore != nullptr)
    //     {
    //         dispatch_release(frame.inFlightSemaphore);
    //         frame.inFlightSemaphore = nullptr;
    //     }
    // }
    //
    // For NS::SharedPtr-only frame resources, clear is enough.
    m_frames.clear();

    m_frameIndex = 0;

    m_device.layer = nullptr;

    m_device.transferQueue = nullptr;
    m_device.computeQueue = nullptr;
    m_device.graphicsQueue = nullptr;
    m_device.device = nullptr;

    m_device.swapchainFormat = MTL::PixelFormatInvalid;
    m_device.depthFormat = MTL::PixelFormatInvalid;

    LOG_INFO("Metal backend shut down");
}

void MetalBackend::CreateSwapchain(WindowHandle* handle)
{
#ifndef PLATFORM_DARWIN
    (void)handle;
    throw std::runtime_error("Metal backend is only supported on Darwin platforms");
#else
    if (m_device.device == nullptr)
    {
        throw std::runtime_error("MetalBackend::CreateSwapchain failed: Metal device is null");
    }

    if (handle == nullptr)
    {
        throw std::runtime_error("MetalBackend::CreateSwapchain failed: WindowHandle is null");
    }

    if (handle->metalLayer == nullptr)
    {
        throw std::runtime_error("MetalBackend::CreateSwapchain failed: WindowHandle::metalLayer is null");
    }

    m_swapchain.layer = CreateMetalLayer(handle);
    m_device.layer = m_swapchain.layer;

    m_swapchain.layer->setDevice(m_device.device.get());
    m_swapchain.layer->setPixelFormat(m_device.swapchainFormat);
    m_swapchain.layer->setFramebufferOnly(true);

    switch (m_frameTimePreference)
    {
        case FrameTimePreference::VSync:
            m_swapchain.layer->setDisplaySyncEnabled(true);
            break;

        case FrameTimePreference::Immediate:
            m_swapchain.layer->setDisplaySyncEnabled(false);
            break;

        default:
            m_swapchain.layer->setDisplaySyncEnabled(true);
            break;
    }

    CGSize drawableSize = m_swapchain.layer->drawableSize();

    uint32_t width = static_cast<uint32_t>(drawableSize.width);
    uint32_t height = static_cast<uint32_t>(drawableSize.height);

    width = std::min<uint32_t>(width, UINT16_MAX);
    height = std::min<uint32_t>(height, UINT16_MAX);

    m_swapchain.extent = {
        static_cast<uint16_t>(width),
        static_cast<uint16_t>(height),
    };

    m_swapchain.nextExtent = m_swapchain.extent;
    m_swapchain.currentTransform = math::Orientation::IDENTITY;
    m_swapchain.isDirty = false;
    m_swapchain.wasRecreated = true;
#endif
}

void MetalBackend::DestroySwapchain()
{
    if (m_swapchain.currentDrawable != nullptr)
    {
        m_swapchain.currentDrawable->release();
        m_swapchain.currentDrawable = nullptr;
    }

    // If you wrap the current CAMetalDrawable texture in an engine TextureHandle,
    // clear/destroy it here.
    //
    // Example:
    //
    // if (m_swapchain.currentColorTexture.IsValid())
    // {
    //     MetalTexture* tex = m_textures.Get(m_swapchain.currentColorTexture);
    //     if (tex != nullptr)
    //     {
    //         tex->texture = nullptr;
    //     }
    //
    //     m_textures.Destroy(m_swapchain.currentColorTexture);
    //     m_swapchain.currentColorTexture = {};
    // }

    if (m_swapchain.currentDepthTexture.IsValid())
    {
        DestroyTexture(m_swapchain.currentDepthTexture);
        m_swapchain.currentDepthTexture = {};
    }

    DestroyMetalLayer(nullptr, m_swapchain.layer);
    m_swapchain.layer = nullptr;

    m_device.layer = nullptr;

    m_swapchain.extent = {};
    m_swapchain.nextExtent = {};
    m_swapchain.currentTransform = math::Orientation::IDENTITY;
    m_swapchain.isDirty = false;
    m_swapchain.wasRecreated = false;
}

}  // namespace oge::graphics::metal
