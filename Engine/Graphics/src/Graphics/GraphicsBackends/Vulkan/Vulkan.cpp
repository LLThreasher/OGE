#include "Vulkan.hpp"

#include <algorithm>
#include <cmath>
#include <set>
#include <string>

#include "VulkanBindingGroups.hpp"
#include "VulkanBuffer.hpp"
#include "VulkanCommandBuffer.hpp"
#include "VulkanFence.hpp"
#include "VulkanFrameBuffer.hpp"
#include "VulkanPipeline.hpp"
#include "VulkanTexture.hpp"

// extern "C" {
#include "vk_mem_alloc.h"
//}

#define LOGGER_NAME "Vulkan"
#include "Engine/Logger.hpp"
#include "Engine/Math.hpp"
#include "Engine/PrintStackTrace.hpp"

namespace OneGame::Engine::Graphics::Vulkan
{
static bool CheckValidationLayerSupport()
{
    uint32_t layerCount;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

    std::vector<VkLayerProperties> layers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, layers.data());

    for (const auto& layer : layers)
    {
        if (strcmp(layer.layerName, "VK_LAYER_KHRONOS_validation") == 0) return true;
    }

    return false;
}

static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                                                    VkDebugUtilsMessageTypeFlagsEXT messageType,
                                                    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
                                                    void* pUserData)
{
    std::string message = pCallbackData->pMessage;

    if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
    {
        LOG_ERROR(message);
#ifdef PLATFORM_WINDOWS
        PrintStackTrace();
#endif
    }
    else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
    {
        LOG_WARN(message);
    }
    else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)
    {
        LOG_INFO(message);
    }
    else
    {
        LOG_DEBUG(message);
    }

    return VK_FALSE;  // do NOT abort Vulkan calls
}

static VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* createInfo,
                                             const VkAllocationCallbacks* allocator,
                                             VkDebugUtilsMessengerEXT* messenger)
{
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");

    if (func != nullptr) return func(instance, createInfo, allocator, messenger);

    return VK_ERROR_EXTENSION_NOT_PRESENT;
}

static void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT messenger,
                                          const VkAllocationCallbacks* allocator)
{
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");

    if (func != nullptr) func(instance, messenger, allocator);
}

VulkanBackend::VulkanBackend() {}

VulkanBackend::~VulkanBackend() {}

uint32_t VulkanBackend::MaxUniformBufferSize() const
{
    VkPhysicalDeviceProperties props{};
    vkGetPhysicalDeviceProperties(m_device.physicalDevice, &props);

    return static_cast<uint32_t>(props.limits.maxUniformBufferRange);
}

uint32_t VulkanBackend::UniformBufferAlignment() const
{
    VkPhysicalDeviceProperties props{};
    vkGetPhysicalDeviceProperties(m_device.physicalDevice, &props);

    return static_cast<uint32_t>(props.limits.minUniformBufferOffsetAlignment);
}

uint32_t VulkanBackend::FramesInFlight() const { return m_frames.size(); }

uint32_t VulkanBackend::CurrentFrameIndex() const { return m_frameIndex; }

float VulkanBackend::SwapchainAspect() const
{
    auto extend = SwapchainExtent();
    return (float)extend.x / (float)extend.y;
}

U16Point2 VulkanBackend::SwapchainExtent() const { return U16Point2{static_cast<uint16_t>(m_swapchain.extent.width), static_cast<uint16_t>(m_swapchain.extent.height)}; }

math::Orientation VulkanBackend::SwapchainPretransform() const { return m_swapchain.currentTransform; }

bool VulkanBackend::SwapchainRecreated() const { return m_swapchain.isDirty; }

struct QueueIndices
{
    std::optional<uint32_t> graphics;
    std::optional<uint32_t> compute;
    std::optional<uint32_t> transfer;
    std::optional<uint32_t> present;

    bool IsComplete() const
    {
        return graphics.has_value() && compute.has_value() && transfer.has_value() && present.has_value();
    }
};

static QueueIndices FindQueues(VkPhysicalDevice device, VkSurfaceKHR surface)
{
    QueueIndices indices;

    uint32_t count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, nullptr);

    std::vector<VkQueueFamilyProperties> families(count);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, families.data());

    for (uint32_t i = 0; i < count; i++)
    {
        auto flags = families[i].queueFlags;

        if ((flags & VK_QUEUE_GRAPHICS_BIT) && !indices.graphics) indices.graphics = i;

        if ((flags & VK_QUEUE_COMPUTE_BIT) && !indices.compute) indices.compute = i;

        if ((flags & VK_QUEUE_TRANSFER_BIT) && !indices.transfer) indices.transfer = i;

        VkBool32 supported = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &supported);
        if (supported == VK_TRUE && !indices.present) indices.present = i;
    }

    if (!indices.transfer.has_value()) indices.transfer = indices.graphics;

    return indices;
}

static bool CheckDeviceExtensionSupport(VkPhysicalDevice device, const std::vector<const char*>& requiredExtensions)
{
    uint32_t extensionCount;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());
    std::set<std::string> requiredSet(requiredExtensions.begin(), requiredExtensions.end());
    for (const auto& extension : availableExtensions)
    {
        requiredSet.erase(extension.extensionName);
    }
    return requiredSet.empty();
}

struct SwapchainSupport
{
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
    VkCompositeAlphaFlagsKHR compositAlphaFlags;
    uint32_t framesInFlight;
};

static void QuerySwapchainSupport(VkPhysicalDevice device, VkSurfaceKHR surface, SwapchainSupport& swapchainSupport)
{
    VkSurfaceCapabilitiesKHR capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &capabilities);
    swapchainSupport.compositAlphaFlags = capabilities.supportedCompositeAlpha;
    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);
    swapchainSupport.formats.resize(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, swapchainSupport.formats.data());
    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);
    swapchainSupport.presentModes.resize(presentModeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, swapchainSupport.presentModes.data());

    swapchainSupport.framesInFlight = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0 && swapchainSupport.framesInFlight > capabilities.maxImageCount)
    {
        swapchainSupport.framesInFlight = capabilities.maxImageCount;
    }
}

static bool IsDeviceSuitable(VkPhysicalDevice device, VkSurfaceKHR surface,
                             const std::vector<const char*>& requiredExtensions, SwapchainSupport& swapchainSupport,
                             QueueIndices& queueIndices)
{
    queueIndices = FindQueues(device, surface);

    bool extensionsSupported = CheckDeviceExtensionSupport(device, requiredExtensions);

    bool swapchainAdequate = false;

    if (extensionsSupported)
    {
        QuerySwapchainSupport(device, surface, swapchainSupport);
        swapchainAdequate = !swapchainSupport.formats.empty() && !swapchainSupport.presentModes.empty();
    }

    return queueIndices.IsComplete() && extensionsSupported && swapchainAdequate;
}

static int RateDevice(VkPhysicalDevice device)
{
    VkPhysicalDeviceProperties props;
    VkPhysicalDeviceFeatures features;

    vkGetPhysicalDeviceProperties(device, &props);
    vkGetPhysicalDeviceFeatures(device, &features);

    int score = 0;

    if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) score += 1000;

    score += props.limits.maxImageDimension2D;

    // if (!features.geometryShader)
    //	return 0;

    return score;
}

static const char* DeviceTypeToString(VkPhysicalDeviceType type)
{
    switch (type)
    {
        case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
            return "Discrete GPU";
        case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
            return "Integrated GPU";
        case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
            return "Virtual GPU";
        case VK_PHYSICAL_DEVICE_TYPE_CPU:
            return "CPU";
        default:
            return "Other";
    }
}

static void PrintPhysicalDeviceInfo(VkPhysicalDevice physicalDevice)
{
    VkPhysicalDeviceProperties props{};
    VkPhysicalDeviceFeatures features{};
    VkPhysicalDeviceMemoryProperties memoryProps{};

    vkGetPhysicalDeviceProperties(physicalDevice, &props);
    vkGetPhysicalDeviceFeatures(physicalDevice, &features);
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProps);

    uint32_t apiVersion = props.apiVersion;

    LOG_INFO("========== GPU ==========");
    LOG_INFO("Name: {}", props.deviceName);
    LOG_INFO("Type: {}", DeviceTypeToString(props.deviceType));
    LOG_INFO("API Version: {}.{}.{}", VK_VERSION_MAJOR(apiVersion), VK_VERSION_MINOR(apiVersion),
             VK_VERSION_PATCH(apiVersion));
    LOG_INFO("Driver Version: {}", props.driverVersion);
    LOG_INFO("Vendor ID: {}", props.vendorID);
    LOG_INFO("Device ID: {}", props.deviceID);

    // ---------------- Memory ----------------
    LOG_INFO("---- Memory Heaps ----");
    for (uint32_t i = 0; i < memoryProps.memoryHeapCount; ++i)
    {
        const auto& heap = memoryProps.memoryHeaps[i];
        double sizeGB = heap.size / (1024.0 * 1024.0 * 1024.0);

        LOG_INFO("Heap {}: {:.2f} GB{}", i, sizeGB,
                 (heap.flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) ? " (Device Local)" : "");
    }

    // ---------------- Queues ----------------
    uint32_t queueCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueCount, nullptr);

    std::vector<VkQueueFamilyProperties> queues(queueCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueCount, queues.data());

    LOG_INFO("---- Queue Families ----");
    for (uint32_t i = 0; i < queueCount; ++i)
    {
        const auto& q = queues[i];

        std::string flags;

        if (q.queueFlags & VK_QUEUE_GRAPHICS_BIT) flags += "Graphics ";
        if (q.queueFlags & VK_QUEUE_COMPUTE_BIT) flags += "Compute ";
        if (q.queueFlags & VK_QUEUE_TRANSFER_BIT) flags += "Transfer ";
        if (q.queueFlags & VK_QUEUE_SPARSE_BINDING_BIT) flags += "Sparse ";

        LOG_INFO("Queue {}: Count={} | {}", i, q.queueCount, flags);
    }

    // ---------------- Important Limits ----------------
    LOG_INFO("---- Limits ----");
    LOG_INFO("Max Image Dimension 2D: {}", props.limits.maxImageDimension2D);
    LOG_INFO("Max Uniform Buffer Range: {}", props.limits.maxUniformBufferRange);
    LOG_INFO("Max Push Constants Size: {}", props.limits.maxPushConstantsSize);
    LOG_INFO("Max Bound Descriptor Sets: {}", props.limits.maxBoundDescriptorSets);

    // ---------------- Features ----------------
    LOG_INFO("---- Features ----");
    LOG_INFO("Geometry Shader: {}", features.geometryShader ? "Yes" : "No");
    LOG_INFO("Tessellation Shader: {}", features.tessellationShader ? "Yes" : "No");
    LOG_INFO("Multi Draw Indirect: {}", features.multiDrawIndirect ? "Yes" : "No");
    LOG_INFO("Sampler Anisotropy: {}", features.samplerAnisotropy ? "Yes" : "No");

    LOG_INFO("========================");
}

static std::tuple<VkPhysicalDevice, SwapchainSupport, QueueIndices> SelectPhysicalDevice(
    VkInstance instance, VkSurfaceKHR surface, const std::vector<const char*>& requiredExtensions)
{
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
    if (deviceCount == 0)
    {
        throw std::runtime_error("Failed to find GPUs with Vulkan support!");
    }
    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

    VkPhysicalDevice bestDevice = VK_NULL_HANDLE;
    SwapchainSupport bestSwapchainSupport;
    QueueIndices bestQueueIndices;
    int bestScore = -1;

    for (const auto& device : devices)
    {
        PrintPhysicalDeviceInfo(device);
    }

    SwapchainSupport currentSwapchainSupport;
    QueueIndices currentQueueIndices;
    for (const auto& device : devices)
    {
        if (!IsDeviceSuitable(device, surface, requiredExtensions, currentSwapchainSupport, currentQueueIndices))
            continue;

        int score = RateDevice(device);

        if (score > bestScore)
        {
            bestScore = score;
            bestDevice = device;
            bestSwapchainSupport = currentSwapchainSupport;
            bestQueueIndices = currentQueueIndices;
        }
    }

    if (bestDevice == VK_NULL_HANDLE) throw std::runtime_error("Failed to find a suitable GPU!");

    return {bestDevice, bestSwapchainSupport, bestQueueIndices};
}

GPUInfo VulkanBackend::GetGPUInfo() const
{
    VkPhysicalDeviceProperties props{};
    VkPhysicalDeviceMemoryProperties memoryProps{};

    vkGetPhysicalDeviceProperties(m_device.physicalDevice, &props);
    vkGetPhysicalDeviceMemoryProperties(m_device.physicalDevice, &memoryProps);

    GPUInfo result{};
    result.name = props.deviceName;
    result.heapCount = memoryProps.memoryHeapCount;
    return result;
}

GPUMemoryUsage VulkanBackend::GetGPUMemoryUsage() const
{
    GPUMemoryUsage result{};

    VmaBudget budgets[VK_MAX_MEMORY_HEAPS];
    vmaGetHeapBudgets(m_device.m_allocator, budgets);

    for (size_t i = 0; i < 16; ++i)
    {
        result.heapUsage[i] = budgets[i].usage;
        result.heapBudget[i] = budgets[i].budget;
    }

    return result;
}

void VulkanBackend::Initialize(const BackendDesc& desc)
{
#if defined(PLATFORM_WINDOWS)
    std::vector<const char*> extensions = {
        VK_KHR_SURFACE_EXTENSION_NAME,
        VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
    };
#elif defined(PLATFORM_ANDROID)
    std::vector<const char*> extensions = {
        VK_KHR_SURFACE_EXTENSION_NAME,
        VK_KHR_ANDROID_SURFACE_EXTENSION_NAME,
    };
#elif defined(PLATFORM_DARWIN)
    std::vector<const char*> extensions = {
        VK_KHR_SURFACE_EXTENSION_NAME,
        VK_EXT_METAL_SURFACE_EXTENSION_NAME,
    };
#else
    std::vector<const char*> extensions;
#endif
#ifdef VULKAN_VALIDATION
    extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif

    // Initialize Vulkan instance, physical device, logical device, swapchain,
    // etc.
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "MyEngine";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "MyEngine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_1;

    {
        const std::vector<const char*> validationLayers = {"VK_LAYER_KHRONOS_validation"};
        VkInstanceCreateInfo createInfo{};

#ifdef VULKAN_VALIDATION
        if (!CheckValidationLayerSupport()) throw std::runtime_error("Validation layers not available");

        createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
        createInfo.ppEnabledLayerNames = validationLayers.data();

        VkValidationFeaturesEXT validationFeatures{};
        validationFeatures.sType = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT;

        VkValidationFeatureEnableEXT enables[] = {VK_VALIDATION_FEATURE_ENABLE_BEST_PRACTICES_EXT,
                                                  VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT};

        validationFeatures.enabledValidationFeatureCount = 2;
        validationFeatures.pEnabledValidationFeatures = enables;

        createInfo.pNext = &validationFeatures;
#else
        createInfo.enabledLayerCount = 0;
#endif

        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;
        createInfo.enabledExtensionCount = extensions.size();
        createInfo.ppEnabledExtensionNames = extensions.data();

        vkCreateInstance(&createInfo, nullptr, &m_device.instance);
    }

#ifdef VULKAN_VALIDATION
    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
    debugCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;

    debugCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                                      VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                      VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;

    debugCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                                  VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                                  VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;

    debugCreateInfo.pfnUserCallback = DebugCallback;

    if (CreateDebugUtilsMessengerEXT(m_device.instance, &debugCreateInfo, nullptr, &m_debugMessenger) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create debug messenger");
    }
#endif

    RecreateSurface(desc.window);

    std::vector deviceExtensions{VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    auto [physicalDevice, swapchainSupport, queueIndices] =
        SelectPhysicalDevice(m_device.instance, m_device.surface, deviceExtensions);
    m_device.physicalDevice = physicalDevice;

    {
        float queuePriority = 1.0f;
        std::set<uint32_t> uniqueQueueFamilies = {queueIndices.graphics.value(), queueIndices.compute.value(),
                                                  queueIndices.transfer.value(), queueIndices.present.value()};

        std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
        for (uint32_t queueFamily : uniqueQueueFamilies)
        {
            VkDeviceQueueCreateInfo queueCreateInfo{};
            queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queueCreateInfo.queueFamilyIndex = queueFamily;
            queueCreateInfo.queueCount = 1;
            queueCreateInfo.pQueuePriorities = &queuePriority;
            queueCreateInfos.push_back(queueCreateInfo);
        }

        VkPhysicalDeviceFeatures supportedFeatures;
        vkGetPhysicalDeviceFeatures(physicalDevice, &supportedFeatures);
        VkPhysicalDeviceFeatures enabledFeatures{};
        if (supportedFeatures.samplerAnisotropy)
        {
            enabledFeatures.samplerAnisotropy = VK_TRUE;
        }

        VkDeviceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        createInfo.queueCreateInfoCount = queueCreateInfos.size();
        createInfo.pQueueCreateInfos = queueCreateInfos.data();
        createInfo.enabledExtensionCount = deviceExtensions.size();
        createInfo.ppEnabledExtensionNames = deviceExtensions.data();
        createInfo.pEnabledFeatures = &enabledFeatures;

        vkCreateDevice(physicalDevice, &createInfo, nullptr, &m_device.device);
        vkGetDeviceQueue(m_device.device, queueIndices.graphics.value(), 0, &m_device.m_graphicsQueue);
        vkGetDeviceQueue(m_device.device, queueIndices.compute.value(), 0, &m_device.m_computeQueue);
        vkGetDeviceQueue(m_device.device, queueIndices.transfer.value(), 0, &m_device.m_transferQueue);
        vkGetDeviceQueue(m_device.device, queueIndices.present.value(), 0, &m_device.m_presentQueue);

        VmaAllocatorCreateInfo allocatorInfo{};
        allocatorInfo.physicalDevice = physicalDevice;
        allocatorInfo.device = m_device.device;
        allocatorInfo.instance = m_device.instance;

        vmaCreateAllocator(&allocatorInfo, &m_device.m_allocator);
    }

    VkSurfaceFormatKHR chosenFormat = swapchainSupport.formats[0];
    for (auto& f : swapchainSupport.formats)
    {
        if (f.format == VK_FORMAT_R8G8B8A8_SRGB && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
        {
            chosenFormat = f;
            break;
        }
        if (f.format == VK_FORMAT_B8G8R8A8_SRGB && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
        {
            chosenFormat = f;
            break;
        }
    }

    m_device.depthFormat = VK_FORMAT_D16_UNORM;

    VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
    for (auto& mode : swapchainSupport.presentModes)
    {
        if (mode == VK_PRESENT_MODE_FIFO_KHR && desc.frameTime == FrameTimePreference::VSync)
        {
            presentMode = mode;
            break;
        }
        if (mode == VK_PRESENT_MODE_MAILBOX_KHR)
        {
            presentMode = mode;
            break;
        }
    }

    m_device.surfaceFormat = chosenFormat;
    m_device.presentMode = presentMode;

    std::array<VkDescriptorPoolSize, 4> poolSizes = {
        VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 32},
        VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 32},
        VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 32},
        VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 64}};

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = (uint32_t)poolSizes.size();
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = 100;

    vkCreateDescriptorPool(m_device.device, &poolInfo, nullptr, &m_descriptorPool);

    m_frames.resize(swapchainSupport.framesInFlight > MAX_FRAMES_IN_FLIGHT ? MAX_FRAMES_IN_FLIGHT
                                                                           : swapchainSupport.framesInFlight);
    LOG_INFO("chosen frames in flight: {}", m_frames.size());

    for (auto& frame : m_frames)
    {
        VkCommandPoolCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        createInfo.queueFamilyIndex = queueIndices.graphics.value();
        createInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        vkCreateCommandPool(m_device.device, &createInfo, nullptr, &frame.pool);
    }

    RecreateSwapchain();
}

void VulkanBackend::RecreateSurface(WindowHandle* handle)
{
    DestroySurface();
    LOG_DEBUG("recreating surface");
    VkResult res;
#ifdef PLATFORM_WINDOWS
    {
        VkWin32SurfaceCreateInfoKHR createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
        createInfo.hwnd = static_cast<HWND>(handle->hwnd);
        createInfo.hinstance = static_cast<HINSTANCE>(handle->hInstance);
        res = vkCreateWin32SurfaceKHR(m_device.instance, &createInfo, nullptr, &m_device.surface);
    }
#elif defined(PLATFORM_ANDROID)
    {
        VkAndroidSurfaceCreateInfoKHR createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
        createInfo.window = static_cast<ANativeWindow*>(handle->nativeWindow);
        res = vkCreateAndroidSurfaceKHR(m_device.instance, &createInfo, nullptr, &m_device.surface);
    }
#elif defined(PLATFORM_DARWIN)
    {
        VkMetalSurfaceCreateInfoEXT createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_METAL_SURFACE_CREATE_INFO_EXT;
        createInfo.pNext = nullptr;
        createInfo.flags = 0;
        createInfo.pLayer = handle->metalLayer;
        res = vkCreateMetalSurfaceEXT(m_device.instance, &createInfo, nullptr, &m_device.surface);
    }
#endif
    if (res != VK_SUCCESS)
    {
        LOG_ERROR("error recreating surface {}", static_cast<uint32_t>(res));
        throw std::runtime_error("vulkan err: " + std::to_string(res));
    }
    LOG_DEBUG("surface recreated");

    RecreateSwapchain();
}

void VulkanBackend::CreateSwapchainRenderPass()
{
    VulkanRenderPassDesc desc{};
    desc.renderTextures[0].format = m_device.surfaceFormat.format;
    desc.renderTextures[0].loadOp = VkAttachmentLoadOp::VK_ATTACHMENT_LOAD_OP_CLEAR;
    desc.renderTextures[0].storeOp = VkAttachmentStoreOp::VK_ATTACHMENT_STORE_OP_STORE;
    desc.colorCount = 1;
    desc.hasDepth = true;
    desc.renderTextures[MaxColorAttachments].format = m_device.depthFormat;
    desc.renderTextures[MaxColorAttachments].loadOp = VkAttachmentLoadOp::VK_ATTACHMENT_LOAD_OP_CLEAR;
    desc.renderTextures[MaxColorAttachments].storeOp = VkAttachmentStoreOp::VK_ATTACHMENT_STORE_OP_DONT_CARE;
    desc.isSwapchain = true;
    auto vkPass = CreateRenderPassInternal(desc);
    m_swapchain.renderPass = m_renderPasses.Create(vkPass);
}

void VulkanBackend::CreateSwapchainFrameBuffers()
{
    m_swapchain.framebuffers.resize(m_swapchain.colorTextures.size());

    for (size_t i = 0; i < m_swapchain.framebuffers.size(); i++)
    {
        FrameBufferDesc desc;
        desc.colorCount = 1;
        desc.hasDepth = true;
        desc.colors[0] = m_swapchain.colorTextures[i];
        desc.depth = m_swapchain.depthTextures[i];

        m_swapchain.framebuffers[i] = CreateFrameBuffer(m_swapchain.renderPass, desc);
    }
}

void VulkanBackend::DestroySwapchain(VkSwapchainKHR& oldSwapchain)
{
    for (auto framebuffer : m_swapchain.framebuffers)
    {
        DestroyFrameBuffer(framebuffer);
    }
    for (int i = 0; i < m_swapchain.depthTextures.size(); i++)
    {
        DestroyTexture(m_swapchain.depthTextures[i]);
    }
    for (auto handle : m_swapchain.colorTextures)
    {
        vkDestroyImageView(m_device.device, m_textures.Get(handle)->view, nullptr);
        m_textures.Destroy(handle);
    }
    DestroyRenderPass(m_swapchain.renderPass);
    vkDestroySwapchainKHR(m_device.device, oldSwapchain, nullptr);
    oldSwapchain = VK_NULL_HANDLE;
}

void VulkanBackend::DestroySurface()
{
    if (m_device.device != VK_NULL_HANDLE)
    {
        vkDeviceWaitIdle(m_device.device);
        LOG_DEBUG("device idle");
        if (m_device.surface != VK_NULL_HANDLE)
        {
            if (m_swapchain.swapchain != VK_NULL_HANDLE)
            {
                DestroySwapchain(m_swapchain.swapchain);
            }
            vkDestroySurfaceKHR(m_device.instance, m_device.surface, nullptr);
            LOG_DEBUG("surface destroyed");
            m_device.surface = VK_NULL_HANDLE;
        }
    }
}

bool VulkanBackend::RecreateSwapchain()
{
    if (m_device.physicalDevice == VK_NULL_HANDLE) return false;

    // Handle swapchain recreation on window resize
    VkSurfaceCapabilitiesKHR capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_device.physicalDevice, m_device.surface, &capabilities);

    if (capabilities.currentExtent.width != UINT32_MAX)
    {
        m_swapchain.extent = capabilities.currentExtent;
    }
    else
    {
        m_swapchain.extent.width = std::clamp(m_swapchain.nextExtent.width, capabilities.minImageExtent.width,
                                              capabilities.maxImageExtent.width);

        m_swapchain.extent.height = std::clamp(m_swapchain.nextExtent.height, capabilities.minImageExtent.height,
                                               capabilities.maxImageExtent.height);
    }

    LOG_DEBUG(
        "Recreate swapchain with requested extend ({}, {}), actual extend "
        "({}, {})",
        m_swapchain.nextExtent.width, m_swapchain.nextExtent.height, m_swapchain.extent.width,
        m_swapchain.extent.height);

    if (m_swapchain.extent.width == 0 || m_swapchain.extent.height == 0)
    {
        LOG_DEBUG("abort RecreateSwapchain: (0, 0) extend");
        return false;
    }

    m_swapchain.nextExtent = m_swapchain.extent;

    uint32_t imageCount = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount)
    {
        imageCount = capabilities.maxImageCount;
    }
    LOG_INFO("chosen min swapchain image count: {}", imageCount);

    VkCompositeAlphaFlagBitsKHR compositeAlpha;

    if (capabilities.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR)
        compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    else if (capabilities.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR)
        compositeAlpha = VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;
    else if (capabilities.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR)
        compositeAlpha = VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR;
    else if (capabilities.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR)
        compositeAlpha = VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR;
    else
        throw std::runtime_error("No supported composite alpha mode");

    {
        vkDeviceWaitIdle(m_device.device);
        VkSwapchainKHR oldSwapchain = m_swapchain.swapchain;
        VkSwapchainCreateInfoKHR createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        createInfo.surface = m_device.surface;

        createInfo.minImageCount = imageCount;
        createInfo.imageFormat = m_device.surfaceFormat.format;
        createInfo.imageColorSpace = m_device.surfaceFormat.colorSpace;
        createInfo.imageExtent = m_swapchain.extent;
        createInfo.imageArrayLayers = 1;
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        createInfo.preTransform = capabilities.currentTransform;
        createInfo.compositeAlpha = compositeAlpha;
        createInfo.presentMode = m_device.presentMode;
        createInfo.clipped = VK_TRUE;
        createInfo.oldSwapchain = oldSwapchain;

        if (createInfo.preTransform & VK_SURFACE_TRANSFORM_ROTATE_90_BIT_KHR)
            m_swapchain.currentTransform = math::Orientation::ROTATE_90;
        else if (createInfo.preTransform & VK_SURFACE_TRANSFORM_ROTATE_270_BIT_KHR)
            m_swapchain.currentTransform = math::Orientation::ROTATE_270;
        else if (createInfo.preTransform & VK_SURFACE_TRANSFORM_ROTATE_180_BIT_KHR)
            m_swapchain.currentTransform = math::Orientation::ROTATE_180;
        else
            m_swapchain.currentTransform = math::Orientation::IDENTITY;

        LOG_DEBUG("currentTransform {}", static_cast<uint32_t>(capabilities.currentTransform));
        LOG_DEBUG("currentAspect {}", SwapchainAspect());

        VK_CHECK_RESULT(vkCreateSwapchainKHR(m_device.device, &createInfo, nullptr, &m_swapchain.swapchain));
        if (oldSwapchain != VK_NULL_HANDLE)
        {
            DestroySwapchain(oldSwapchain);
        }
    }

    m_swapchain.isDirty = true;
    LOG_DEBUG("swapchain created with format {}, color space {}", static_cast<uint32_t>(m_device.surfaceFormat.format),
              static_cast<uint32_t>(m_device.surfaceFormat.colorSpace));

    uint32_t swapchainImageCount;
    vkGetSwapchainImagesKHR(m_device.device, m_swapchain.swapchain, &swapchainImageCount, nullptr);
    LOG_INFO("chosen final swapchain image count: {}", swapchainImageCount);

    std::vector<VkImage> images(swapchainImageCount);
    vkGetSwapchainImagesKHR(m_device.device, m_swapchain.swapchain, &swapchainImageCount, images.data());

    std::vector<VkImageView> views(swapchainImageCount);
    for (size_t i = 0; i < images.size(); ++i)
    {
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = images[i];
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = m_device.surfaceFormat.format;

        viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        vkCreateImageView(m_device.device, &viewInfo, nullptr, &views[i]);
    }
    LOG_DEBUG("swapchain images created");

    m_swapchain.colorTextures.resize(images.size());
    for (size_t i = 0; i < images.size(); ++i)
    {
        VulkanTexture tex;
        tex.width = m_swapchain.extent.width;
        tex.height = m_swapchain.extent.height;
        tex.format = m_device.surfaceFormat.format;
        tex.image = images[i];
        tex.view = views[i];
        m_swapchain.colorTextures[i] = m_textures.Create(tex);
    }

    m_swapchain.depthTextures.resize(swapchainImageCount);
    for (int i = 0; i < swapchainImageCount; ++i)
    {
        VulkanTexture depthTexture;
        CreateTextureInternal(m_swapchain.extent.width, m_swapchain.extent.height, 1, 1, 1,
                              VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT,
                              m_device.depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT, depthTexture);
        m_swapchain.depthTextures[i] = m_textures.Create(depthTexture);
    }
    LOG_DEBUG("swapchain depth image created");

    CreateSwapchainRenderPass();
    LOG_DEBUG("swapchain render pass created");
    CreateSwapchainFrameBuffers();
    LOG_DEBUG("swapchain frame buffer created");
    CreateSyncObjects(swapchainImageCount);
    LOG_DEBUG("Sync objects created");
    return true;
}

void VulkanBackend::Resize(uint32_t windowWidth, uint32_t windowHeight)
{
    m_swapchain.nextExtent = {windowWidth, windowHeight};
}

void VulkanBackend::WaitDeviceIdle() { vkDeviceWaitIdle(m_device.device); }

void VulkanBackend::Shutdown()
{
    LOG_DEBUG("suhtdown wait device");
    vkDeviceWaitIdle(m_device.device);

    LOG_DEBUG("suhtdown destroy swapchain");
    DestroySwapchain(m_swapchain.swapchain);

    LOG_DEBUG("suhtdown pool frame buffers");
    while (m_frameBuffers.Size() > 0) DestroyFrameBuffer(m_frameBuffers.Poll());
    LOG_DEBUG("suhtdown pool render passes");
    while (m_renderPasses.Size() > 0) DestroyRenderPass(m_renderPasses.Poll());
    LOG_DEBUG("suhtdown pool pipelines");
    while (m_pipelines.Size() > 0) DestroyPipeline(m_pipelines.Poll());

    LOG_DEBUG("suhtdown pool binding groups");
    while (m_bindingGroups.Size() > 0) DestroyBindingGroup(m_bindingGroups.Poll());
    while (m_bindingGroupLayouts.Size() > 0) DestroyBindingGroupLayout(m_bindingGroupLayouts.Poll());
    vkDestroyDescriptorPool(m_device.device, m_descriptorPool, nullptr);

    LOG_DEBUG("suhtdown pool fences");
    while (m_fences.Size() > 0)
    {
        auto handle = m_fences.Poll();
        vkDestroyFence(m_device.device, m_fences.Get(handle)->fence, nullptr);
        m_fences.Destroy(handle);
    }

    LOG_DEBUG("suhtdown pool textures");
    while (m_textures.Size() > 0) DestroyTexture(m_textures.Poll());
    while (m_buffers.Size() > 0) DestroyBuffer(m_buffers.Poll());

    LOG_DEBUG("suhtdown frame data");
    for (auto& frame : m_frames)
    {
        vkDestroyCommandPool(m_device.device, frame.pool, nullptr);
        vkDestroyFence(m_device.device, frame.inFlightFence, nullptr);
        vkDestroySemaphore(m_device.device, frame.imageAvailableAndTransferComplete[0], nullptr);
        vkDestroySemaphore(m_device.device, frame.imageAvailableAndTransferComplete[1], nullptr);
        vkDestroySemaphore(m_device.device, frame.renderFinished, nullptr);
    }
    for (auto& semaphore : m_imagesFinishRender)
    {
        vkDestroySemaphore(m_device.device, semaphore, nullptr);
    }

    LOG_DEBUG("suhtdown allocator");
    if (m_device.m_allocator != nullptr)
    {
        vmaDestroyAllocator(m_device.m_allocator);
        m_device.m_allocator = nullptr;
    }

    LOG_DEBUG("suhtdown device");
    if (m_device.device != VK_NULL_HANDLE)
    {
        vkDestroyDevice(m_device.device, nullptr);
        m_device.device = VK_NULL_HANDLE;
    }

    LOG_DEBUG("suhtdown surface");
    if (m_device.surface != VK_NULL_HANDLE)
    {
        vkDestroySurfaceKHR(m_device.instance, m_device.surface, nullptr);
        m_device.surface = VK_NULL_HANDLE;
    }

#ifdef VULKAN_VALIDATION
    LOG_DEBUG("suhtdown debug messenger");
    DestroyDebugUtilsMessengerEXT(m_device.instance, m_debugMessenger, nullptr);
#endif

    if (m_device.instance != VK_NULL_HANDLE)
    {
        LOG_DEBUG("suhtdown device");
        vkDestroyInstance(m_device.instance, nullptr);
        m_device.instance = VK_NULL_HANDLE;
    }
    LOG_DEBUG("suhtdown finished");
}

#undef max

GPUFrameBufferHandle VulkanBackend::GetCurrentFrameBuffer() const { return m_swapchain.framebuffers[m_imageIndex]; }

GPURenderPassHandle VulkanBackend::GetCurrentRenderPass() const { return m_swapchain.renderPass; }

BeginFrameAction VulkanBackend::BeginFrame()
{
    FrameData& frame = m_frames[m_frameIndex];

    VkDevice& device = m_device.device;
    VkSwapchainKHR& swapchain = m_swapchain.swapchain;

    // Wait for previous frame to finish
    vkWaitForFences(m_device.device, 1, &frame.inFlightFence, VK_TRUE, UINT64_MAX);
    
    // Acquire next swapchain image
    VkResult result = vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, frame.imageAvailableAndTransferComplete[0],
                                            VK_NULL_HANDLE, &m_imageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR)
    {
        LOG_DEBUG("recreating swapchain: VK_ERROR_OUT_OF_DATE_KHR");
        if (RecreateSwapchain())
            return BeginFrameAction::SkipFrame;
        else
            return BeginFrameAction::RecreateSurface;
    }
    else if (result == VK_ERROR_SURFACE_LOST_KHR)
    {
        return BeginFrameAction::RecreateSurface;
    }
    else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
    {
        LOG_ERROR("vkAcquireNextImageKHR error: {}", static_cast<uint32_t>(result));
        throw std::runtime_error("Failed to acquire swapchain image");
    }

    // avoid drawing on active image
    if (m_imagesInFlight[m_imageIndex] != VK_NULL_HANDLE)
    {
        vkWaitForFences(m_device.device, 1, &m_imagesInFlight[m_imageIndex], VK_TRUE, UINT64_MAX);
        m_imagesInFlight[m_imageIndex] = VK_NULL_HANDLE;
    }

    vkResetFences(m_device.device, 1, &frame.inFlightFence);

    // Reset command pool
    vkResetCommandPool(device, frame.pool, 0);
    frame.cmdUsedCount[0] = 0;
    frame.cmdUsedCount[1] = 0;
    frame.cmdUsedCount[2] = 0;
    frame.cmdUsedCount[3] = 0;

    m_imagesInFlight[m_imageIndex] = frame.inFlightFence;
    return BeginFrameAction::Continue;
}

EndFrameAction VulkanBackend::EndFrame()
{
    FrameData& frame = m_frames[m_frameIndex];
    VkSwapchainKHR& swapchain = m_swapchain.swapchain;

    {
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

        auto queueIdx = static_cast<uint32_t>(QueueType::Transfer);
        auto& transferCmdBuffers = frame.vkCmdBuffers[queueIdx];
        auto& frame = m_frames[m_frameIndex];
        submitInfo.waitSemaphoreCount = 0;
        submitInfo.pWaitSemaphores = VK_NULL_HANDLE;
        submitInfo.pWaitDstStageMask = VK_NULL_HANDLE;
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = &frame.imageAvailableAndTransferComplete[1];
        submitInfo.commandBufferCount = frame.cmdUsedCount[queueIdx];
        submitInfo.pCommandBuffers = transferCmdBuffers.data();

        vkQueueSubmit(m_device.m_transferQueue, 1, &submitInfo, VK_NULL_HANDLE);
    }

    {
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

        auto queueIdx = static_cast<uint32_t>(QueueType::Present);
        auto& presentCmdBuffers = frame.vkCmdBuffers[queueIdx];
        auto& frame = m_frames[m_frameIndex];
        static VkPipelineStageFlags waitStage[2] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                                    VK_PIPELINE_STAGE_TRANSFER_BIT};
        submitInfo.waitSemaphoreCount = 2;
        submitInfo.pWaitSemaphores = frame.imageAvailableAndTransferComplete;
        submitInfo.pWaitDstStageMask = waitStage;
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = &m_imagesFinishRender[m_imageIndex];
        submitInfo.commandBufferCount = frame.cmdUsedCount[queueIdx];
        submitInfo.pCommandBuffers = presentCmdBuffers.data();

        vkQueueSubmit(m_device.m_presentQueue, 1, &submitInfo, frame.inFlightFence);
    }

    // Present
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &m_imagesFinishRender[m_imageIndex];

    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &swapchain;
    presentInfo.pImageIndices = &m_imageIndex;

    VkResult result = vkQueuePresentKHR(m_device.m_presentQueue, &presentInfo);

    m_frameIndex = (m_frameIndex + 1) % m_frames.size();
    m_swapchain.isDirty = false;

    if (result == VK_ERROR_SURFACE_LOST_KHR)
    {
        DestroySurface();
        return EndFrameAction::RecreateSurface;
    }
    else if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR ||
             m_swapchain.nextExtent.width != m_swapchain.extent.width ||
             m_swapchain.nextExtent.height != m_swapchain.extent.height)
    {
        LOG_DEBUG(
            "recreating swapchain: VK_ERROR_OUT_OF_DATE_KHR={}, "
            "VK_SUBOPTIMAL_KHR={}, {}",
            result == VK_ERROR_OUT_OF_DATE_KHR, result == VK_SUBOPTIMAL_KHR,
            m_swapchain.nextExtent.width != m_swapchain.extent.width ||
                m_swapchain.nextExtent.height != m_swapchain.extent.height);
        if (RecreateSwapchain())
            return EndFrameAction::RecreateSwapchain;
        else
            return EndFrameAction::RecreateSurface;
    }
    else if (result != VK_SUCCESS)
    {
        LOG_ERROR("vkQueuePresentKHR error: {}", static_cast<uint32_t>(result));
        throw std::runtime_error("Failed to present swapchain image");
    }
    return EndFrameAction::Continue;
}

void VulkanBackend::CreateSyncObjects(int swapImageCount)
{
    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    // Important: start signaled so first frame doesn't stall

    for (uint32_t i = 0; i < m_frames.size(); i++)
    {
        if (m_frames[i].imageAvailableAndTransferComplete[0] != VK_NULL_HANDLE)
        {
            vkDestroySemaphore(m_device.device, m_frames[i].imageAvailableAndTransferComplete[0], nullptr);
        }

        if (vkCreateSemaphore(m_device.device, &semaphoreInfo, nullptr,
                              &m_frames[i].imageAvailableAndTransferComplete[0]) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create imageAvailable semaphore");
        }
        LOG_DEBUG("frame {} image available semaphore {}", i, (void*)m_frames[i].imageAvailableAndTransferComplete[0]);

        if (m_frames[i].imageAvailableAndTransferComplete[1] != VK_NULL_HANDLE)
        {
            vkDestroySemaphore(m_device.device, m_frames[i].imageAvailableAndTransferComplete[1], nullptr);
        }

        if (vkCreateSemaphore(m_device.device, &semaphoreInfo, nullptr,
                              &m_frames[i].imageAvailableAndTransferComplete[1]) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create imageAvailable semaphore");
        }
        LOG_DEBUG("frame {} transfer complete semaphore {}", i,
                  (void*)m_frames[i].imageAvailableAndTransferComplete[1]);

        if (m_frames[i].renderFinished != VK_NULL_HANDLE)
        {
            vkDestroySemaphore(m_device.device, m_frames[i].renderFinished, nullptr);
        }

        if (vkCreateSemaphore(m_device.device, &semaphoreInfo, nullptr, &m_frames[i].renderFinished) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create renderFinished semaphore");
        }
        LOG_DEBUG("frame {} render finished semaphore {}", i, (void*)m_frames[i].renderFinished);

        if (m_frames[i].inFlightFence != VK_NULL_HANDLE)
        {
            vkDestroyFence(m_device.device, m_frames[i].inFlightFence, nullptr);
        }

        if (vkCreateFence(m_device.device, &fenceInfo, nullptr, &m_frames[i].inFlightFence) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create inFlight fence");
        }
        LOG_DEBUG("frame {} in flight fence {}", i, (void*)m_frames[i].inFlightFence);
    }

    m_imagesInFlight.resize(swapImageCount);
    m_imagesFinishRender.resize(swapImageCount);
    for (uint32_t i = 0; i < swapImageCount; ++i)
    {
        if (m_imagesFinishRender[i] != VK_NULL_HANDLE)
        {
            vkDestroySemaphore(m_device.device, m_imagesFinishRender[i], nullptr);
        }

        m_imagesInFlight[i] = VK_NULL_HANDLE;
        if (vkCreateSemaphore(m_device.device, &semaphoreInfo, nullptr, &m_imagesFinishRender[i]) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create imageAvailable semaphore");
        }
    }
}

ICommandList& VulkanBackend::CreateCommandList(QueueType queueType)
{
    assert(queueType == QueueType::Present || queueType == QueueType::Transfer);
    FrameData& frame = m_frames[m_frameIndex];

    uint32_t queueIndex = static_cast<uint32_t>(queueType);
    VulkanCommandBuffer* cmd;

    if (frame.cmdUsedCount[queueIndex] < frame.cmdBuffers[queueIndex].size())
    {
        cmd = &frame.cmdBuffers[queueIndex][frame.cmdUsedCount[queueIndex]];
        cmd->Clear();
    }
    else
    {
        LOG_DEBUG("allocate cmd list");
        // allocate new one
        VkCommandBufferAllocateInfo alloc{};
        alloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        alloc.commandPool = frame.pool;
        alloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        alloc.commandBufferCount = 1;

        VkCommandBuffer vkCmd;
        vkAllocateCommandBuffers(m_device.device, &alloc, &vkCmd);
        frame.vkCmdBuffers[queueIndex].emplace_back(vkCmd);
        cmd = &frame.cmdBuffers[queueIndex].emplace_back(queueType, m_device.device, vkCmd, this);
    }

    frame.cmdUsedCount[queueIndex]++;

    return *reinterpret_cast<ICommandList*>(cmd);
}
}  // namespace OneGame::Engine::Graphics::Vulkan
