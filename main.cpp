#include <chrono>
#include <windows.h>
#include "Engine/EngineLogger.hpp"
#include "Engine/Graphics/IGraphicsBackend.hpp"
#include "Engine/Graphics/TestRenderer.hpp"

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

double getTimeSeconds()
{
    static auto start = std::chrono::high_resolution_clock::now();

    auto now = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = now - start;

    return elapsed.count();
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
        "OneGame",
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

    auto backend = CreateBackend(BackendType::Vulkan);
    try
    {
        WindowHandle handle{};
        handle.hInstance = GetModuleHandle(nullptr);
        handle.hwnd = hwnd;
        backend->Resize(width, height);
        backend->Initialize(BackendDesc{ &handle });
    }
    catch (const std::exception& e)
    {
        MessageBoxA(hwnd, e.what(), "Vulkan Error", MB_OK);
        return -1;
    }

    LOG_INFO("Backend created");

    MSG msg = {};
    bool running = true;

    //auto testRenderer = TestRenderer();
    //auto testRenderer = TestRendererRotateTriangle();
    auto testRenderer = TestRendererCubeWithMVP();
    try
    {
        //bool stepFrame = false;
        testRenderer.Initialize(backend.get());
    }
    catch (const std::exception& e)
    {
        MessageBoxA(hwnd, e.what(), "Vulkan Error", MB_OK);
        return -1;
    }

    double frameTime = 0.0;
    int frameCount = 0;
    double cumTime = 0.0;
    auto lastTime = std::chrono::high_resolution_clock::now();
    while (msg.message != WM_QUIT)
    {

        auto now = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsed = now - lastTime;
        lastTime = now;

        float deltaTime = static_cast<float>(elapsed.count());

        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT)
            {
                running = false;
                break;
            }

            //if (msg.message == WM_KEYDOWN && msg.wParam == VK_SPACE)
            //{
            //    bool wasDown = (msg.lParam & (1 << 30)) != 0;

            //    if (!wasDown)
            //    {
            //        stepFrame = true;
            //    }
            //}

            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        if (!running)
            break;

        //if (stepFrame)
        //{
        //    backend->BeginFrame();
        //    testRenderer.Render(backend.get(), 0.f);
        //    backend->EndFrame();

        //    stepFrame = false;  // consume the press
        //}
        backend->BeginFrame();
        
        testRenderer.Render(backend.get(), deltaTime);

        backend->EndFrame();
    }
    backend->Shutdown();
}



