#include "oge/__api__.h"

#include <cstring>
#include <memory>
#include <vector>

#include "oge/graphics/vulkan/create_backend.hpp"
#ifdef OGE_USE_METAL
#include "oge/graphics/metal/create_backend.hpp"
#endif
#include "oge/platform/sdl3/create_window.hpp"
#include "oge/platform/window.hpp"
#include "oge/platform/window_app.hpp"

struct Window
{
    std::unique_ptr<oge::platform::Window> ptr;
};

struct WindowApp
{
    std::unique_ptr<oge::platform::WindowApp> ptr;
};

struct Backend
{
    std::unique_ptr<oge::graphics::IGraphicsBackend> ptr;
};

// 2. The C API Implementation bindings
extern "C"
{
    Window_t* OGE_Window_Create(const char* backend, const char* name,
                                size_t width, size_t height)
    {
        return reinterpret_cast<Window_t*>(new Window{
            oge::platform::CreateWindow(backend, name, width, height)});
    }

    void OGE_Window_Destroy(Window_t* instance)
    {
        if (instance)
        {
            // Cast back to the real class type and safely delete
            delete reinterpret_cast<Window*>(instance);
        }
    }

    void OGE_Window_Run(Window_t* instance, WindowApp_t* app)
    {
        if (instance && app)
        {
            instance->ptr->Run(*app->ptr.get());
        }
    }

    WindowApp_t* OGE_App_Create(const char* name)
    {
        return reinterpret_cast<WindowApp_t*>(
            new WindowApp{oge::platform::CreateWindowApp(name)});
    }

    void OGE_App_Destroy(WindowApp_t* instance)
    {
        if (instance)
        {
            delete reinterpret_cast<WindowApp*>(instance);
        }
    }

    void OGE_Init(void)
    {
        oge::platform::RegisterWindowFactory("SDL3", &oge::platform::sdl3::CreateSDL3Window);
    }

    // --- Graphics Backend -------------------------------------------------

    Backend_t* OGE_Backend_Create(const char* name)
    {
        if (std::strcmp(name, "Vulkan") == 0)
        {
            return reinterpret_cast<Backend_t*>(new Backend{
                oge::graphics::vulkan::CreateVulkanBackend()});
        }
#ifdef OGE_USE_METAL
        if (std::strcmp(name, "Metal") == 0)
        {
            return reinterpret_cast<Backend_t*>(new Backend{
                oge::graphics::metal::CreateMetalBackend()});
        }
#endif
        return nullptr;
    }

    void* OGE_Backend_Release(Backend_t* instance)
    {
        auto* b = reinterpret_cast<Backend*>(instance);
        auto* raw = b->ptr.release();  // transfer ownership out
        delete b;                       // destroy wrapper
        return raw;
    }

    void OGE_Backend_Destroy(Backend_t* instance)
    {
        delete reinterpret_cast<Backend*>(instance);
    }
}  // extern "C"
