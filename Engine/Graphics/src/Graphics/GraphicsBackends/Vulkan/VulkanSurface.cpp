#include "Vulkan.hpp"

#include <vulkan/vulkan.h>

namespace OneGame::Engine::Graphics::Vulkan
{
VkResult CreateSurface(WindowHandle* handle, VkInstance instance, VkSurfaceKHR& surface)
{
    VkResult res;
#ifdef PLATFORM_WINDOWS
    {
        VkWin32SurfaceCreateInfoKHR createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
        createInfo.hwnd = static_cast<HWND>(handle->hwnd);
        createInfo.hinstance = static_cast<HINSTANCE>(handle->hInstance);
        res = vkCreateWin32SurfaceKHR(instance, &createInfo, nullptr, &surface);
    }
#elif defined(PLATFORM_ANDROID)
    {
        VkAndroidSurfaceCreateInfoKHR createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
        createInfo.window = static_cast<ANativeWindow*>(handle->nativeWindow);
        res = vkCreateAndroidSurfaceKHR(instance, &createInfo, nullptr, &surface);
    }
#elif defined(PLATFORM_DARWIN)
    {
        VkMetalSurfaceCreateInfoEXT createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_METAL_SURFACE_CREATE_INFO_EXT;
        createInfo.pNext = nullptr;
        createInfo.flags = 0;
        createInfo.pLayer = handle->metalLayer;
        res = vkCreateMetalSurfaceEXT(instance, &createInfo, nullptr, &surface);
    }
#else
#error
#endif
    return res;
}
}