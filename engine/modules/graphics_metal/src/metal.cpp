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

#ifdef OGE_HAS_SPIRV_CROSS
#include <spirv_cross/spirv_msl.hpp>
#endif

#include "oge/graphics/backend.hpp"
#include "oge/graphics/configs.hpp"

#include "metal_command_buffer.hpp"

#define LOGGER_NAME "Metal"
#include "oge/log.hpp"
#include "oge/math.hpp"
#include "oge/platform/stacktrace.hpp"

// Autorelease pool helpers (metal_autorelease.mm).
extern "C" {
void* MetalAutoreleasePoolPush();
void MetalAutoreleasePoolPop(void* pool);
}

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

    if (result.get() == nullptr)
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

    if (m_device.device.get() == nullptr)
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

    if (m_device.device.get() == nullptr)
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

        case TextureFormat::Depth32Float:
            return MTL::PixelFormatDepth32Float;

        case TextureFormat::Depth24FloatStencil8:
            return MTL::PixelFormatDepth24Unorm_Stencil8;

        case TextureFormat::Depth32FloatStencil8:
            return MTL::PixelFormatDepth32Float_Stencil8;

        default:
            LOG_ERROR("Unsupported TextureFormat {}", static_cast<uint32_t>(format));
            throw std::runtime_error("Unsupported TextureFormat for Metal");
    }
}

MTL::TextureUsage MetalBackend::ToMetalTextureUsage(
    TextureUsage usage) const
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

    if ((usage & TextureUsage::ColorAttachment) ==
        TextureUsage::ColorAttachment)
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
        case MemoryUsage::CPUToGPU:
        case MemoryUsage::GPUToCPU:
            options |= MTL::ResourceStorageModeShared; break;
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

MTL::VertexFormat MetalBackend::ToMetalVertexFormat(
    VertexAttributeFormat fmt, uint32_t* outSize) const
{
    switch (fmt)
    {
        case VertexAttributeFormat::Float32:
            *outSize = 4;  return MTL::VertexFormatFloat;
        case VertexAttributeFormat::Float32x2:
            *outSize = 8;  return MTL::VertexFormatFloat2;
        case VertexAttributeFormat::Float32x3:
            *outSize = 12; return MTL::VertexFormatFloat3;
        case VertexAttributeFormat::Float32x4:
            *outSize = 16; return MTL::VertexFormatFloat4;
        case VertexAttributeFormat::Uint32:
            *outSize = 4;  return MTL::VertexFormatUInt;
        case VertexAttributeFormat::Uint32x2:
            *outSize = 8;  return MTL::VertexFormatUInt2;
        case VertexAttributeFormat::Uint32x3:
            *outSize = 12; return MTL::VertexFormatUInt3;
        case VertexAttributeFormat::Uint32x4:
            *outSize = 16; return MTL::VertexFormatUInt4;
        case VertexAttributeFormat::Uint16:
            *outSize = 2;  return MTL::VertexFormatUShort;
        case VertexAttributeFormat::Uint16x2:
            *outSize = 4;  return MTL::VertexFormatUShort2;
        case VertexAttributeFormat::Uint8:
            *outSize = 1;  return MTL::VertexFormatUChar;
        case VertexAttributeFormat::UniformUint16:
            *outSize = 2;  return MTL::VertexFormatUShortNormalized;
        case VertexAttributeFormat::UniformUint16x2:
            *outSize = 4;  return MTL::VertexFormatUShort2Normalized;
        case VertexAttributeFormat::UniformUint8x4:
            *outSize = 4;  return MTL::VertexFormatUChar4Normalized;
        default:
            *outSize = 16; return MTL::VertexFormatFloat4;
    }
}

MTL::PrimitiveType MetalBackend::ToMetalPrimitiveType(
    PrimitiveTopology topology) const
{
    switch (topology)
    {

        case PrimitiveTopology::LineList:
            return MTL::PrimitiveTypeLine;


        case PrimitiveTopology::TriangleList:
            return MTL::PrimitiveTypeTriangle;


        default:
            return MTL::PrimitiveTypeTriangle;
    }
}

MTL::IndexType MetalBackend::ToMetalIndexType(IndexFormat format) const
{
    switch (format)
    {
        case IndexFormat::Uint16:
            return MTL::IndexTypeUInt16;

        case IndexFormat::Uint32:
            return MTL::IndexTypeUInt32;

        default:
            return MTL::IndexTypeUInt32;
    }
}

MTL::CompareFunction MetalBackend::ToMetalCompareFunction(
    DepthCompareOp function) const
{
    switch (function)
    {
        case DepthCompareOp::Never:
            return MTL::CompareFunctionNever;

        case DepthCompareOp::Less:
            return MTL::CompareFunctionLess;

        case DepthCompareOp::Equal:
            return MTL::CompareFunctionEqual;

        case DepthCompareOp::LessEqual:
            return MTL::CompareFunctionLessEqual;

        case DepthCompareOp::Greater:
            return MTL::CompareFunctionGreater;

        case DepthCompareOp::NotEqual:
            return MTL::CompareFunctionNotEqual;

        case DepthCompareOp::GreaterEqual:
            return MTL::CompareFunctionGreaterEqual;

        case DepthCompareOp::Always:
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
        case FrontFace::CW:
            return MTL::WindingClockwise;

        case FrontFace::CCW:
            return MTL::WindingCounterClockwise;

        default:
            return MTL::WindingCounterClockwise;
    }
}

bool MetalBackend::CreateDevice()
{
    m_device.device = SelectMetalDevice();

    if (m_device.device.get() == nullptr)
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
    if (m_device.device.get() == nullptr)
    {
        throw std::runtime_error("Cannot create Metal command queues without device");
    }

    m_device.graphicsQueue =
        NS::TransferPtr(m_device.device->newCommandQueue());
    m_device.computeQueue =
        NS::TransferPtr(m_device.device->newCommandQueue());
    m_device.transferQueue =
        NS::TransferPtr(m_device.device->newCommandQueue());

    if (m_device.graphicsQueue.get() == nullptr)
    {
        throw std::runtime_error("Failed to create Metal graphics command queue");
    }

    if (m_device.computeQueue.get() == nullptr)
    {
        throw std::runtime_error("Failed to create Metal compute command queue");
    }

    if (m_device.transferQueue.get() == nullptr)
    {
        throw std::runtime_error("Failed to create Metal transfer command queue");
    }
}

CA::MetalLayer* RetainMetalLayer(WindowHandle* handle)
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
    if (desc.window.metalLayer == nullptr)
        throw std::runtime_error(
            "MetalBackend::Initialize: metalLayer is null");

    LOG_INFO("Initializing Metal backend");
    m_frameIndex = 0;

    if (!CreateDevice())
        throw std::runtime_error(
            "MetalBackend::Initialize: failed to create Metal device");

    CreateCommandQueues();
    CreateSwapchain(&desc.window);

    uint32_t n = MAX_FRAMES_IN_FLIGHT > 3 ? 3u
                 : static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
    m_frames.resize(n);
    for (auto& f : m_frames)
        f.inFlightSemaphore = dispatch_semaphore_create(1);

    m_swapchain.wasRecreated = true;
    m_swapchain.isDirty = false;

    // Default "no depth" DSS for passes without depth testing.
    {
        auto* d = MTL::DepthStencilDescriptor::alloc()->init();
        d->setDepthCompareFunction(MTL::CompareFunctionAlways);
        d->setDepthWriteEnabled(false);
        m_noDepthDSS = NS::TransferPtr(
            m_device.device->newDepthStencilState(d));
        d->release();
    }

    LOG_INFO("Metal backend initialized");
}

void MetalBackend::RecreateSurface(WindowHandle& handle)
{
    if (handle.metalLayer == nullptr)
        throw std::runtime_error("MetalBackend::RecreateSurface: metalLayer is null");

    LOG_INFO("Recreating Metal surface");
    WaitDeviceIdle();
    DestroySwapchain();
    CreateSwapchain(&handle);
    m_swapchain.wasRecreated = true;
    m_swapchain.isDirty = false;
    LOG_INFO("Metal surface recreated");
}

void MetalBackend::WaitDeviceIdle()
{
    if (m_device.graphicsQueue.get() == nullptr) return;
    MTL::CommandBuffer* cb = m_device.graphicsQueue->commandBuffer();
    if (cb == nullptr) return;
    cb->commit();
    cb->waitUntilCompleted();
    cb->release();
}

void MetalBackend::Shutdown()
{
    LOG_INFO("Shutting down Metal backend");
    WaitDeviceIdle();
    DestroySwapchain();

    for (auto& f : m_frames)
    {
        delete f.commandList;
        f.commandList = nullptr;

        if (f.inFlightSemaphore != nullptr)
        {
            dispatch_release(f.inFlightSemaphore);
            f.inFlightSemaphore = nullptr;
        }
    }
    m_frames.clear();
    m_frameIndex = 0;
    m_device.layer = nullptr;
    m_device.transferQueue = nullptr;
    m_device.computeQueue = nullptr;
    m_device.graphicsQueue = nullptr;
    m_defaultSampler = nullptr;
    m_noDepthDSS = nullptr;
    m_device.device = nullptr;
    m_device.swapchainFormat = MTL::PixelFormatInvalid;
    m_device.depthFormat = MTL::PixelFormatInvalid;
    LOG_INFO("Metal backend shut down");
}

// --- Frame loop --------------------------------------------------------

BeginFrameAction MetalBackend::BeginFrame()
{
    // Push a fresh autorelease pool for this frame.
    m_framePool = MetalAutoreleasePoolPush();

    auto& frame = m_frames[m_frameIndex];
    if (frame.inFlightSemaphore != nullptr)
        dispatch_semaphore_wait(frame.inFlightSemaphore, DISPATCH_TIME_FOREVER);

    if (m_swapchain.isDirty && m_swapchain.layer != nullptr)
        RecreateSwapchain();

    if (!AcquireNextDrawable())
        return BeginFrameAction::RecreateSurface;

    m_swapchain.wasRecreated = false;
    return BeginFrameAction::Continue;
}

EndFrameAction MetalBackend::EndFrame()
{
    // Metal's ObjC runtime autoreleases temporary objects.  In a C++/SDL3
    // app there is no Cocoa run loop, so the pool is never drained.
    // Drain it at the end of each frame.
    auto& frame = m_frames[m_frameIndex];

    if (frame.commandBuffer.get() != nullptr)
    {
        frame.commandBuffer->commit();
        frame.commandBuffer = nullptr;
    }

    if (m_swapchain.currentDrawable != nullptr)
    {
        auto* cb = m_device.graphicsQueue->commandBuffer();
        cb->presentDrawable(m_swapchain.currentDrawable);
        cb->commit();
        cb->waitUntilCompleted();
        cb->release();
        m_swapchain.currentDrawable = nullptr;
        m_swapchain.currentColorTexture = {};
    }

    if (frame.inFlightSemaphore != nullptr)
        dispatch_semaphore_signal(frame.inFlightSemaphore);

    m_frameIndex = (m_frameIndex + 1) % static_cast<uint32_t>(m_frames.size());

    // Drain this frame's autorelease pool.
    MetalAutoreleasePoolPop(m_framePool);
    m_framePool = nullptr;

    return EndFrameAction::Continue;
}

// --- Command list ------------------------------------------------------

ICommandList& MetalBackend::CreateCommandList(QueueType type)
{
    auto& frame = m_frames[m_frameIndex];

    // Reuse the same MTL::CommandBuffer for all encoders within a frame
    // (blit for upload, then render for drawing).  This avoids per-queue
    // command-buffer churn which grows Metal's internal pool.
    if (frame.commandBuffer.get() == nullptr)
    {
        // Always use the graphics queue — transfer/upload blits also work
        // on the graphics queue and this keeps everything in one CB.
        frame.commandBuffer = NS::RetainPtr(
            m_device.graphicsQueue->commandBuffer());
    }

    // Delete the previous command list (from upload or prior frame).
    delete frame.commandList;

    auto* cmd = new MetalCommandBuffer(frame.commandBuffer.get(), *this);
    frame.commandList = cmd;
    return *cmd;
}

// --- Buffers -----------------------------------------------------------

GPUBufferHandle MetalBackend::CreateBuffer(const BufferDesc& desc,
                                           void** stagingMemory)
{
    MTL::ResourceOptions opts = ToMetalResourceOptions(desc.memory);
    MTL::Buffer* buf = m_device.device->newBuffer(desc.size, opts);
    if (buf == nullptr)
        throw std::runtime_error("Metal: failed to create buffer");

    MetalBuffer result{};
    result.buffer = NS::RetainPtr(buf);
    result.desc = desc;
    if (stagingMemory != nullptr)
        *stagingMemory = buf->contents();
    return m_buffers.Create(result);
}

void MetalBackend::DestroyBuffer(GPUBufferHandle handle)
{
    auto* b = m_buffers.Get(handle);
    if (b != nullptr) b->buffer = nullptr;
    m_buffers.Destroy(handle);
}

void MetalBackend::FlushStagingBufferRanges(
    const std::span<GPUBufferSpan> ranges)
{
    // Apple Silicon uses unified memory — shared buffers are automatically
    // coherent between CPU and GPU.  didModifyRange only applies to managed
    // storage mode which doesn't exist on Apple GPUs.
    (void)ranges;
}

// --- Textures ----------------------------------------------------------

GPUTextureHandle MetalBackend::CreateTexture(const TextureDesc& desc)
{
    auto pixelFmt = ToMetalPixelFormat(desc.format);

    // Use texture2DDescriptor for 2D textures, or manually configure
    // a 2D-array when layers > 1.
    auto* mtlDesc = MTL::TextureDescriptor::alloc()->init();
    mtlDesc->setPixelFormat(pixelFmt);
    mtlDesc->setWidth(desc.width);
    mtlDesc->setHeight(desc.height);
    mtlDesc->setDepth(1);
    mtlDesc->setMipmapLevelCount(std::max(1u, desc.mipLevels));
    mtlDesc->setArrayLength(std::max(1u, desc.layers));
    mtlDesc->setTextureType(desc.layers > 1
                                ? MTL::TextureType2DArray
                                : MTL::TextureType2D);
    mtlDesc->setUsage(ToMetalTextureUsage(desc.usage));
    mtlDesc->setStorageMode(ToMetalStorageMode(MemoryUsage::GPUOnly));

    MTL::Texture* tex = m_device.device->newTexture(mtlDesc);
    mtlDesc->release();
    if (tex == nullptr)
        throw std::runtime_error("Metal: failed to create texture");

    MetalTexture result{};
    result.texture = NS::RetainPtr(tex);
    result.desc = desc;
    return m_textures.Create(result);
}

void MetalBackend::DestroyTexture(GPUTextureHandle handle)
{
    DestroyTextureInternal(handle);
}

// --- Pipelines ---------------------------------------------------------

GPUPipelineHandle MetalBackend::CreateGraphicsPipeline(
    const GraphicsPipelineDesc& desc)
{
    auto* rpDesc = MTL::RenderPipelineDescriptor::alloc()->init();

    auto vertFn = desc.vertexShader.empty()
                      ? nullptr
                      : CreateShaderFunction(desc.vertexShader, "vertexMain");
    if (vertFn.get() == nullptr)
    {
        LOG_ERROR("Metal: vertex shader failed — Metal needs MSL, not SPIR-V");
        rpDesc->release();
        return {};
    }
    rpDesc->setVertexFunction(vertFn.get());

    if (!desc.fragmentShader.empty())
    {
        auto fragFn = CreateShaderFunction(desc.fragmentShader,
                                           "fragmentMain");
        if (fragFn.get() != nullptr)
            rpDesc->setFragmentFunction(fragFn.get());
    }

    // Vertex descriptor — map vertexLayout to Metal vertex attributes.
    if (!desc.vertexLayout.empty())
    {
        auto* vd = MTL::VertexDescriptor::alloc()->init();
        uint32_t byteOffset = 0;
        for (size_t i = 0; i < desc.vertexLayout.size(); ++i)
        {
            uint32_t size = 0;
            auto* attr = vd->attributes()->object(i);
            attr->setBufferIndex(
                MetalCommandBuffer::kVertexBufferSlot);
            attr->setOffset(byteOffset);
            attr->setFormat(
                ToMetalVertexFormat(desc.vertexLayout[i], &size));
            byteOffset += size;
        }
        auto* layout = vd->layouts()->object(
            MetalCommandBuffer::kVertexBufferSlot);
        layout->setStride(byteOffset);
        layout->setStepFunction(MTL::VertexStepFunctionPerVertex);
        rpDesc->setVertexDescriptor(vd);
        vd->release();
    }

    // Default color attachment (BGRA8Unorm sRGB, matching swapchain).
    auto* ca = rpDesc->colorAttachments()->object(0);
    ca->setPixelFormat(MTL::PixelFormatBGRA8Unorm_sRGB);
    if (desc.blending)
    {
        ca->setBlendingEnabled(true);
        ca->setSourceRGBBlendFactor(MTL::BlendFactorSourceAlpha);
        ca->setDestinationRGBBlendFactor(
            MTL::BlendFactorOneMinusSourceAlpha);
        ca->setSourceAlphaBlendFactor(MTL::BlendFactorOne);
        ca->setDestinationAlphaBlendFactor(
            MTL::BlendFactorOneMinusSourceAlpha);
    }

    // Always match the depth format our render pass provides.
    rpDesc->setDepthAttachmentPixelFormat(
        ToMetalPixelFormat(TextureFormat::Depth32Float));

    // Depth-stencil state — only when depth testing is enabled.
    NS::SharedPtr<MTL::DepthStencilState> dss;
    if (desc.depthTest)
    {
        auto* dssDesc = MTL::DepthStencilDescriptor::alloc()->init();
        dssDesc->setDepthCompareFunction(
            ToMetalCompareFunction(desc.depthCompareOp));
        dssDesc->setDepthWriteEnabled(desc.writeDepth);
        dss = NS::RetainPtr(
            m_device.device->newDepthStencilState(dssDesc));
        dssDesc->release();
    }

    NS::Error* error = nullptr;
    MTL::RenderPipelineState* pso =
        m_device.device->newRenderPipelineState(rpDesc, &error);
    rpDesc->release();

    if (pso == nullptr)
    {
        auto errMsg = error != nullptr
                          ? error->localizedDescription()->utf8String()
                          : "unknown";
        LOG_ERROR("Metal: failed to create render pipeline (vs={}, fs={}): {}",
                  desc.vertexShader.size(), desc.fragmentShader.size(), errMsg);
        if (error != nullptr) error->release();
        return {};
    }

    LOG_INFO("Metal: pipeline created OK (vs={} fs={} layout={})",
             desc.vertexShader.size(), desc.fragmentShader.size(),
             desc.vertexLayout.size());

    MetalPipeline result{};
    result.renderPipeline = NS::RetainPtr(pso);
    result.depthStencilState = dss;
    result.graphicsDesc = desc;
    result.primitiveType = ToMetalPrimitiveType(desc.topology);
    return m_pipelines.Create(result);
}

GPUPipelineHandle MetalBackend::CreateComputePipeline(
    const ComputePipelineDesc& desc)
{
    auto* cpDesc = MTL::ComputePipelineDescriptor::alloc()->init();
    cpDesc->setComputeFunction(
        CreateShaderFunction(desc.shader, "kernelMain").get());

    NS::Error* error = nullptr;
    MTL::ComputePipelineState* pso =
        m_device.device->newComputePipelineState(
            cpDesc, MTL::PipelineOptionNone, nullptr, &error);
    cpDesc->release();

    if (pso == nullptr)
    {
        LOG_ERROR("Metal: failed to create compute pipeline");
        if (error != nullptr) error->release();
        return {};
    }

    MetalPipeline result{};
    result.computePipeline = NS::RetainPtr(pso);
    result.computeDesc = desc;
    return m_pipelines.Create(result);
}

void MetalBackend::DestroyPipeline(GPUPipelineHandle handle)
{
    auto* p = m_pipelines.Get(handle);
    if (p != nullptr)
    {
        p->renderPipeline = nullptr;
        p->computePipeline = nullptr;
        p->depthStencilState = nullptr;
    }
    m_pipelines.Destroy(handle);
}

// --- Binding groups ----------------------------------------------------

GPUBindingGroupLayoutHandle MetalBackend::CreateBindingGroupLayout(
    const BindingGroupLayoutDesc& desc)
{
    MetalBindingGroupLayout r{};
    r.desc = desc;
    return m_bindingGroupLayouts.Create(r);
}

void MetalBackend::DestroyBindingGroupLayout(GPUBindingGroupLayoutHandle h)
{
    m_bindingGroupLayouts.Destroy(h);
}

GPUBindingGroupHandle MetalBackend::CreateBindingGroup(
    const BindingGroupDesc& desc)
{
    MetalBindingGroup r{};
    r.desc = desc;
    return m_bindingGroups.Create(r);
}

void MetalBackend::DestroyBindingGroup(GPUBindingGroupHandle h)
{
    m_bindingGroups.Destroy(h);
}

// --- Fences ------------------------------------------------------------

GPUFenceHandle MetalBackend::CreateFence(bool signaled)
{
    MetalFence r{};
    r.signaled = signaled;
    return m_fences.Create(r);
}

void MetalBackend::WaitForFence(GPUFenceHandle handle)
{
    auto* f = m_fences.Get(handle);
    if (f != nullptr && !f->signaled &&
        f->commandBuffer.get() != nullptr)
    {
        f->commandBuffer->waitUntilCompleted();
        f->signaled = true;
    }
}

bool MetalBackend::IsFenceSignaled(GPUFenceHandle handle)
{
    auto* f = m_fences.Get(handle);
    return f != nullptr && f->signaled;
}

void MetalBackend::ResetFence(GPUFenceHandle handle)
{
    auto* f = m_fences.Get(handle);
    if (f != nullptr)
    {
        f->signaled = false;
        f->commandBuffer = nullptr;
    }
}

// --- Internal helpers --------------------------------------------------

void MetalBackend::DestroyTextureInternal(GPUTextureHandle handle)
{
    auto* t = m_textures.Get(handle);
    if (t != nullptr) t->texture = nullptr;
    m_textures.Destroy(handle);
}

bool MetalBackend::AcquireNextDrawable()
{
    if (m_swapchain.layer == nullptr) return false;

    if (m_swapchain.currentDrawable != nullptr)
    {
        m_swapchain.currentDrawable->release();
        m_swapchain.currentDrawable = nullptr;
    }

    m_swapchain.currentDrawable = m_swapchain.layer->nextDrawable();
    return m_swapchain.currentDrawable != nullptr;
}

bool MetalBackend::RecreateSwapchain()
{
    auto* cb = m_device.graphicsQueue->commandBuffer();
    cb->commit();
    cb->waitUntilCompleted();
    cb->release();

    if (m_swapchain.nextExtent.x > 0 && m_swapchain.nextExtent.y > 0)
    {
        m_swapchain.extent = m_swapchain.nextExtent;
        m_swapchain.nextExtent = {};
    }

    m_swapchain.isDirty = false;
    m_swapchain.wasRecreated = true;
    return true;
}

NS::SharedPtr<MTL::Function> MetalBackend::CreateShaderFunction(
    const std::vector<char>& code, const char* entry)
{
    if (code.empty()) return nullptr;

    // SPIR-V → MSL conversion, and the actual entry-point name.
    std::string mslSource;
    std::string realEntry = entry;  // may be overridden by SPIR-V reflection
#ifdef OGE_HAS_SPIRV_CROSS
    {
        auto* words = reinterpret_cast<const uint32_t*>(code.data());
        size_t wordCount = code.size() / sizeof(uint32_t);
        std::vector<uint32_t> spirv(words, words + wordCount);
        try
        {
            spirv_cross::CompilerMSL compiler(std::move(spirv));
            auto opts = compiler.get_msl_options();
            compiler.set_msl_options(opts);

            auto entryPoints = compiler.get_entry_points_and_stages();
            if (!entryPoints.empty())
                realEntry = entryPoints[0].name;

            mslSource = compiler.compile();
        }
        catch (const std::exception& e)
        {
            LOG_ERROR("Metal: SPIR-V -> MSL failed: {}", e.what());
            return nullptr;
        }
    }
#else
    mslSource.assign(code.data(), code.size());
#endif

    NS::Error* error = nullptr;
    auto* src = NS::String::string(mslSource.c_str(), NS::UTF8StringEncoding);
    auto* lib = m_device.device->newLibrary(src, nullptr, &error);
    if (lib == nullptr)
    {
        auto* desc = error ? error->localizedDescription()->utf8String() : "?";
        LOG_ERROR("Metal: shader library failed: {}", desc);
        if (error) error->release();
        return nullptr;
    }

    // SPIRV-Cross renames GLSL "main" → "main0" in MSL.  Try a few
    // common names; the first non-null result wins.
    auto tryFn = [&](const char* name) -> MTL::Function* {
        return lib->newFunction(
            NS::String::string(name, NS::UTF8StringEncoding));
    };
    auto* fn = tryFn(realEntry.c_str());           // reflected SPIR-V name
    if (fn == nullptr) fn = tryFn("main0");        // SPIRV-Cross MSL convention
    if (fn == nullptr) fn = tryFn("main");         // GLSL convention
    if (fn == nullptr) fn = tryFn(entry);          // caller's request
    if (fn == nullptr)
    {
        LOG_ERROR("Metal: entry point not found (tried '{}', 'main0', 'main', '{}')",
                  realEntry, entry);
        lib->release();
        return nullptr;
    }
    lib->release();
    return NS::RetainPtr(fn);
}

GPUTextureHandle MetalBackend::CreateDepthTexture(uint32_t width,
                                                   uint32_t height)
{
    auto pixelFmt = ToMetalPixelFormat(TextureFormat::Depth32Float);
    auto* mtlDesc = MTL::TextureDescriptor::texture2DDescriptor(
        pixelFmt, width, height, false);
    mtlDesc->setUsage(MTL::TextureUsageRenderTarget);
    mtlDesc->setStorageMode(MTL::StorageModePrivate);

    MTL::Texture* tex = m_device.device->newTexture(mtlDesc);
    mtlDesc->release();
    if (tex == nullptr) return {};

    MetalTexture result{};
    result.texture = NS::RetainPtr(tex);
    result.desc.format = TextureFormat::Depth32Float;
    result.desc.width = width;
    result.desc.height = height;
    result.desc.depth = 1;
    result.isDepthTexture = true;
    return m_textures.Create(result);
}

}  // namespace oge::graphics::metal
