// Why this is a C++20 module (not a regular header/.cpp pair):
//
// Defining VK_USE_PLATFORM_XLIB_KHR before #include <vulkan/vulkan.h> causes
// vulkan.h to pull in vulkan_xlib.h, which in turn includes the X11 headers
// (Xlib.h / X.h). Those headers define preprocessor macros such as None,
// Always, Bool, Status, True, False, and Success. The macros collide with
// engine enums (e.g. BufferUsage::None, CullMode::None, CompareOp::Always)
// and break any translation unit that sees both X11 and our graphics headers.
//
// A module keeps that #include <vulkan/vulkan.h> (and the X11 headers it
// transitively brings in) inside the global module fragment below. Macros do
// not leak across `import`, so importers can call CreateX11Surface / use
// vkXlibSurfaceExtensionName without X11 pollution, while we still use the
// official Vulkan/Xlib declarations (no hand-maintained struct copies).

module;

#define VK_USE_PLATFORM_XLIB_KHR
#include <vulkan/vulkan.h>

export module VulkanX11Wsi;

export namespace OneGame::Engine::Graphics::Vulkan
{
inline constexpr const char* vkXlibSurfaceExtensionName = VK_KHR_XLIB_SURFACE_EXTENSION_NAME;
VkResult CreateX11Surface(void* display, unsigned long window, VkInstance instance, VkSurfaceKHR* surface)
{
    VkXlibSurfaceCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR;
    createInfo.pNext = nullptr;
    createInfo.flags = 0;
    createInfo.dpy = static_cast<Display*>(display);
    createInfo.window = static_cast<Window>(window);
    return vkCreateXlibSurfaceKHR(instance, &createInfo, nullptr, surface);
}
}  // namespace OneGame::Engine::Graphics::Vulkan
