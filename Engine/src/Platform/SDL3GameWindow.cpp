#ifdef USE_SDL3

#ifdef PLATFORM_WINDOWS
#include <windows.h>
#elif defined(PLATFORM_ANDROID)
#include <android/native_window.h>
#endif

#include <SDL3/SDL.h>
#include <SDL3/SDL_mouse.h>

#include "Engine/Platform/IGameWindow.hpp"
#include "Engine/Timer.hpp"
#include "Engine/GameApp.hpp"

#define LOGGER_NAME "Engine"
#include "Engine/Logger.hpp"

namespace OneGame::Engine
{
    static KeyCode map[256] = {};

    KeyCode GetEngineKey(SDL_Keycode sdlKey)
    {
        if (sdlKey < 256)
        {
            return map[sdlKey];
        }
        return KeyCode::KY_UNASSIGNED;
    }

    class SDL3GameWindow : public IGameWindow
    {
    public:
        SDL3GameWindow(std::string name, int width, int height);

        void Run(GameApp&) override;
    private:
        void PollEvents();
        Graphics::WindowHandle GetCurrentWindow();

        InputSystem m_input;
        SDL_Window* m_window;
        Timer m_timer;
        bool m_shouldClose = false;
    };

    SDL3GameWindow::SDL3GameWindow(std::string name, int width, int height)
    {
        map[SDLK_ESCAPE] = KeyCode::KY_ESCAPE;
        map[SDLK_SPACE] = KeyCode::KY_SPACE;
        map[SDLK_0] = KeyCode::KY_0;
        map[SDLK_1] = KeyCode::KY_1;
        map[SDLK_2] = KeyCode::KY_2;
        map[SDLK_3] = KeyCode::KY_3;
        map[SDLK_4] = KeyCode::KY_4;
        map[SDLK_5] = KeyCode::KY_5;
        map[SDLK_6] = KeyCode::KY_6;
        map[SDLK_7] = KeyCode::KY_7;
        map[SDLK_8] = KeyCode::KY_8;
        map[SDLK_9] = KeyCode::KY_9;
        map[SDLK_A] = KeyCode::KY_A;
        map[SDLK_B] = KeyCode::KY_B;
        map[SDLK_C] = KeyCode::KY_C;
        map[SDLK_D] = KeyCode::KY_D;
        map[SDLK_E] = KeyCode::KY_E;
        map[SDLK_F] = KeyCode::KY_F;
        map[SDLK_G] = KeyCode::KY_G;
        map[SDLK_H] = KeyCode::KY_H;
        map[SDLK_I] = KeyCode::KY_I;
        map[SDLK_J] = KeyCode::KY_J;
        map[SDLK_K] = KeyCode::KY_K;
        map[SDLK_L] = KeyCode::KY_L;
        map[SDLK_M] = KeyCode::KY_M;
        map[SDLK_N] = KeyCode::KY_N;
        map[SDLK_O] = KeyCode::KY_O;
        map[SDLK_P] = KeyCode::KY_P;
        map[SDLK_Q] = KeyCode::KY_Q;
        map[SDLK_R] = KeyCode::KY_R;
        map[SDLK_S] = KeyCode::KY_S;
        map[SDLK_T] = KeyCode::KY_T;
        map[SDLK_U] = KeyCode::KY_U;
        map[SDLK_V] = KeyCode::KY_V;
        map[SDLK_W] = KeyCode::KY_W;
        map[SDLK_X] = KeyCode::KY_X;
        map[SDLK_Y] = KeyCode::KY_Y;
        map[SDLK_Z] = KeyCode::KY_Z;

        LOG_INFO("SDL3 GameWindow Created");
        // 1. Initialize SDL
        SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);

        // 2. Create window with SDL_WINDOW_VULKAN flag
        m_window = SDL_CreateWindow(name.c_str(), width, height, 0);
    }

    Graphics::WindowHandle SDL3GameWindow::GetCurrentWindow()
    {
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
        return handle;
    }

    void SDL3GameWindow::Run(GameApp& app)
    {
        SDL_ShowWindow(m_window);
        try
        {
            auto handle = GetCurrentWindow();
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
        bool waitingSurface = false;
        SDL_Event event;

        double timeAccumulator = 0.0f;
        while (!readyToClose)
        {
            m_input.NewFrame();
            while (SDL_PollEvent(&event))
            {
                switch (event.type)
                {
                case SDL_EVENT_QUIT:
                    readyToClose = true;
                    break;
                case SDL_EVENT_WINDOW_RESTORED:
                {
                    auto handle = GetCurrentWindow();
                    app.OnWindowRecreate(&handle);
                    waitingSurface = false;
                }
                    break;
                case SDL_EVENT_KEY_DOWN:
                    m_input.SetKey(GetEngineKey(event.key.key), true);
                    break;
                case SDL_EVENT_KEY_UP:
                    m_input.SetKey(GetEngineKey(event.key.key), false);
                    break;
                case SDL_EVENT_MOUSE_MOTION:
                    m_input.SetMouseDelta(event.motion.xrel, event.motion.yrel);
                    m_input.SetMousePosition(event.motion.x, event.motion.y);
                    break;
                }
            }

            if (waitingSurface)
                continue;

            auto appFrameAction = app.Update(m_timer.Tick(), m_input);
            if ((appFrameAction & AppFrameAction::WaitSurface) == AppFrameAction::WaitSurface)
            {
                waitingSurface = true;
            }
            if ((appFrameAction & AppFrameAction::WrapMouse) == AppFrameAction::WrapMouse)
            {
                SDL_SetWindowRelativeMouseMode(m_window, true);
            }
            else if ((appFrameAction & AppFrameAction::UnwrapMouse) == AppFrameAction::UnwrapMouse)
            {
                SDL_SetWindowRelativeMouseMode(m_window, false);
            }
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
