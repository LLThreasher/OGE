#ifdef PLATFORM_WINDOWS

#include <windows.h>

#include "Engine/Platform/IGameWindow.hpp"

#define LOGGER_NAME "Engine"
#include "Engine/Input/InputSystem.hpp"
#include "Engine/Graphics/IGraphicsBackend.hpp"
#include "Engine/WindowedGameApp.hpp"
#include "Engine/Input/Keyboard.hpp"
#include "Engine/Logger.hpp"
#include "Engine/Timer.hpp"

namespace OneGame::Engine
{
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

class Win32GameWindow : public IGameWindow
{
   public:
    Win32GameWindow(std::string name, int width, int height);

    void Run(GameGraphicApp&) override;

   private:
    void PollEvents();

    InputSystem m_input;
    HWND m_hwnd;
    Timer m_timer;
    bool m_shouldClose = false;
};

Win32GameWindow::Win32GameWindow(std::string name, int width, int height)
{
    WNDCLASS wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.lpszClassName = "MyWindowClass";

    RegisterClass(&wc);

    m_hwnd = CreateWindowEx(0, "MyWindowClass", "OneGame", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 1280, 720,
                            nullptr, nullptr, GetModuleHandle(nullptr), nullptr);
}

void Win32GameWindow::Run(GameGraphicApp& app)
{
    ShowWindow(m_hwnd, SW_SHOW);

    RECT rect;
    GetClientRect(m_hwnd, &rect);

    uint32_t width = rect.right - rect.left;
    uint32_t height = rect.bottom - rect.top;

    Graphics::WindowHandle handle{};
    handle.hInstance = GetModuleHandle(nullptr);
    handle.hwnd = m_hwnd;

    try
    {
        app.Initialize(&handle);
    }
    catch (const std::exception& e)
    {
        MessageBoxA(m_hwnd, e.what(), "Vulkan Error", MB_OK);
        return;
    }

    while (!m_shouldClose)
    {
        PollEvents();

        float dt = m_timer.Tick();

        app.Update(dt, m_input);
    }

    app.Shutdown();
}

void Win32GameWindow::PollEvents()
{
    MSG msg = {};
    while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
    {
        if (msg.message == WM_QUIT)
        {
            m_shouldClose = true;
            break;
        }

        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

#ifndef USE_SDL3
std::unique_ptr<IGameWindow> CreateGameWindow(const std::string& title, int width, int height)
{
    return std::unique_ptr<IGameWindow>(title, width, height);
}
#endif
}  // namespace OneGame::Engine
#endif
