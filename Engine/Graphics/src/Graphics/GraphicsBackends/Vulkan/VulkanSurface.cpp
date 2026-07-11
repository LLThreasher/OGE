#include <vulkan/vulkan.h>

#include "Vulkan.hpp"
#ifdef PLATFORM_LINUX
import VulkanX11Wsi;
#endif

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
#elif defined(PLATFORM_LINUX)
    {
        if (handle->isWayland)
        {
            VkWaylandSurfaceCreateInfoKHR createInfo{};

            createInfo.sType = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR;
            createInfo.display = static_cast<struct wl_display*>(handle->wayland.display);
            createInfo.surface = static_cast<struct wl_surface*>(handle->wayland.surface);
            res = vkCreateWaylandSurfaceKHR(instance, &createInfo, nullptr, &surface);
        }
        else
        {
            res = CreateX11Surface(handle->x11.display, handle->x11.window,instance, &surface);
        }
    }
#else
#error
#endif
    return res;
}
}  // namespace OneGame::Engine::Graphics::Vulkan
