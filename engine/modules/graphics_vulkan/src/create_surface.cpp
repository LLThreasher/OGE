#ifdef PLATFORM_LINUX
#define VK_USE_PLATFORM_XLIB_KHR
#elif defined(PLATFORM_WINDOWS)
#define VK_USE_PLATFORM_WIN32_KHR
#endif
#include <vulkan/vulkan.h>

// With VK_USE_PLATFORM_XLIB_KHR, vulkan.h pulls in X11/Xlib.h, which
// defines macros (None, Always, Status, Bool, Success, event types, ...)
// that collide with C++ identifiers in the engine headers below — drop
// them so everything after this point parses cleanly.
#undef None
#undef Always
#undef Status
#undef Bool
#undef Success
#undef CursorShape
#undef Expose
#undef KeyPress
#undef KeyRelease
#undef FocusIn
#undef FocusOut
#undef FontChange
#undef Unsorted

#include "oge/platform/window_handle.hpp"
#include "vulkan.hpp"

namespace oge::graphics::vulkan
{
#ifdef PLATFORM_LINUX
static VkResult CreateX11Surface(void* display, unsigned long window,
                                 VkInstance instance, VkSurfaceKHR* surface)
{
    VkXlibSurfaceCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR;
    createInfo.pNext = nullptr;
    createInfo.flags = 0;
    createInfo.dpy = static_cast<Display*>(display);
    createInfo.window = static_cast<Window>(window);
    return vkCreateXlibSurfaceKHR(instance, &createInfo, nullptr, surface);
}
#endif

VkResult CreateSurface(WindowHandle* handle, VkInstance instance,
                       VkSurfaceKHR& surface)
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
        res =
            vkCreateAndroidSurfaceKHR(instance, &createInfo, nullptr, &surface);
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

            createInfo.sType =
                VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR;
            createInfo.display =
                static_cast<struct wl_display*>(handle->wayland.display);
            createInfo.surface =
                static_cast<struct wl_surface*>(handle->wayland.surface);
            res = vkCreateWaylandSurfaceKHR(instance, &createInfo, nullptr,
                                            &surface);
        }
        else
        {
            res = CreateX11Surface(handle->x11.display, handle->x11.window,
                                   instance, &surface);
        }
    }
#else
#error
#endif
    return res;
}
}  // namespace oge::graphics::vulkan
