#include <windows.h>
#include "Engine/Logger.hpp"
#include "Engine/Graphics/IGraphicsBackend.hpp"

using namespace OneGame::Engine;
using namespace OneGame::Engine::Graphics;

LRESULT CALLBACK WindowProc(
    HWND hwnd,
    UINT uMsg,
    WPARAM wParam,
    LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

int main() {
    WNDCLASS wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.lpszClassName = "MyWindowClass";

    RegisterClass(&wc);

    HWND hwnd = CreateWindowEx(
        0,
        "MyWindowClass",
        "Vulkan Window",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        1280, 720,
        nullptr,
        nullptr,
        GetModuleHandle(nullptr),
        nullptr
    );

    ShowWindow(hwnd, SW_SHOW);

    RECT rect;
    GetClientRect(hwnd, &rect);

    uint32_t width = rect.right - rect.left;
    uint32_t height = rect.bottom - rect.top;

    Logger::Init();
    LOG_INFO("Logger initialized");

    auto backend = CreateBackend(BackendType::Vulkan);
    try
    {
        WindowHandle handle{};
        handle.hInstance = GetModuleHandle(nullptr);
        handle.hwnd = hwnd;
        backend->Initialize(BackendDesc{ &handle });
    }
    catch (const std::exception& e)
    {
        MessageBoxA(hwnd, e.what(), "Vulkan Error", MB_OK);
        return -1;
    }

    MSG msg = {};

    while (msg.message != WM_QUIT)
    {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else
        {
            // Your engine update + render
        }
    }
}


