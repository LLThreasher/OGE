#pragma once

namespace oge::platform
{

struct WindowHandle
{
#ifdef PLATFORM_WINDOWS
    void* hInstance;
    void* hwnd;
#elif defined(PLATFORM_ANDROID)
    void* nativeWindow;
#elif defined(PLATFORM_DARWIN)
    const void* metalLayer;
#elif defined(PLATFORM_LINUX)
    bool isWayland;
    union
    {
        struct
        {
            void* display;
            unsigned long window;
        } x11;
        struct
        {
            void* display;
            void* surface;
        } wayland;
    };
#else
#error
#endif
};

}  // namespace OneGame::Engine::Graphics
