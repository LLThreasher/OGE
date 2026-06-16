#ifdef USE_SDL3

#ifdef PLATFORM_WINDOWS
#include <windows.h>
#elif defined(PLATFORM_ANDROID)
#include <android/native_window.h>
#endif

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

#include "Engine/Platform/IGameWindow.hpp"
#include "Engine/Timer.hpp"
#include "Engine/GameApp.hpp"

#define LOGGER_NAME "Engine"
#include "Engine/Logger.hpp"

namespace OneGame::Engine
{
    class SDL3GameWindow : public IGameWindow
    {
    public:
        SDL3GameWindow(std::string name, int width, int height);

        void Run(GameApp&) override;
    private:
        void PollEvents();

        SDL_Window* m_window;
        Timer m_timer;
        bool m_shouldClose = false;
    };

    SDL3GameWindow::SDL3GameWindow(std::string name, int width, int height)
    {
        // 1. Initialize SDL
        SDL_Init(SDL_INIT_VIDEO);

        // 2. Create window with SDL_WINDOW_VULKAN flag
        m_window = SDL_CreateWindow(name.c_str(), width, height, 0);
    }

    void SDL3GameWindow::Run(GameApp& app)
    {
        SDL_ShowWindow(m_window);
        Graphics::WindowHandle handle{};
#ifdef PLATFORM_WINDOWS
        SDL_PropertiesID props = SDL_GetWindowProperties(m_window);
        HWND hwnd = (HWND)SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL);
        handle.hInstance = GetModuleHandle(nullptr);
        handle.hwnd = hwnd;
#else
        SDL_PropertiesID props = SDL_GetWindowProperties(m_window);
        ANativeWindow* nativeWindow = (ANativeWindow*)SDL_GetPointerProperty(
            props,
            SDL_PROP_WINDOW_ANDROID_WINDOW_POINTER,
            NULL
        );
        handle.nativeWindow = nativeWindow;
#endif
        try
        {
            app.Initialize(&handle);
        }
        catch (const std::exception& e)
        {
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Vulkan Error", e.what(), m_window);
            SDL_DestroyWindow(m_window);
            SDL_Quit();
            return;
        }

        bool readyToClose = false;
        SDL_Event event;

        double timeAccumulator = 0.0f;
        while (!readyToClose)
        {
            while (SDL_PollEvent(&event))
            {
                switch (event.type)
                {
                case SDL_EVENT_QUIT:
                    readyToClose = true;
                    break;
                }
            }

            app.Update(m_timer.Tick());
        }

        app.Shutdown();
        SDL_DestroyWindow(m_window);
        SDL_Quit();
    }

    std::unique_ptr<IGameWindow>
        CreateGameWindow(const std::string& title,
            int width,
            int height) {
        return std::unique_ptr<IGameWindow>(new SDL3GameWindow(title, width, height));
    }
}
#endif
