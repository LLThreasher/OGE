#include "sdl3_window.hpp"

namespace oge::platform::sdl3
{

#ifdef PLATFORM_DARWIN
const void* GetMetalLayer(SDL_Window* sdlWindow);
#endif

WindowHandle SDL3GameWindow::GetCurrentWindow()
{
    WindowHandle handle{};
#ifdef PLATFORM_WINDOWS
    SDL_PropertiesID props = SDL_GetWindowProperties(m_window);
    handle.hInstance = GetModuleHandle(nullptr);
    handle.hwnd = SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL);
#elif defined(PLATFORM_ANDROID)
    SDL_PropertiesID props = SDL_GetWindowProperties(m_window);
    handle.nativeWindow = SDL_GetPointerProperty(props, SDL_PROP_WINDOW_ANDROID_WINDOW_POINTER, NULL);
#elif defined(PLATFORM_DARWIN)
    handle.metalLayer = GetMetalLayer(m_window);
#elif defined(PLATFORM_LINUX)
    SDL_PropertiesID props = SDL_GetWindowProperties(m_window);
    const char* driver = SDL_GetCurrentVideoDriver();
    if (driver && SDL_strcmp(driver, "wayland") == 0)
    {
        handle.isWayland = true;
        handle.wayland.display = SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WAYLAND_DISPLAY_POINTER, nullptr);
        handle.wayland.surface = SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WAYLAND_SURFACE_POINTER, nullptr);
    }
    else
    {
        handle.isWayland = false;
        handle.x11.display = SDL_GetPointerProperty(props, SDL_PROP_WINDOW_X11_DISPLAY_POINTER, nullptr);
        handle.x11.window =
            static_cast<unsigned long>(SDL_GetNumberProperty(props, SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0));
    }
#else
#error
#endif
    return handle;
}

}