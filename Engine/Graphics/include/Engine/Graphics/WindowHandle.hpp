#pragma once

namespace OneGame::Engine::Graphics
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
#else
#error
#endif
};

}  // namespace OneGame::Engine::Graphics
