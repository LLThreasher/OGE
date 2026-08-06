#pragma once

#include <SDL3/SDL.h>
#include <memory>

#include "oge/platform/window.hpp"
#include "oge/platform/window_app.hpp"
#include "oge/platform/window_handle.hpp"
#include "oge/timer.hpp"

namespace oge::platform::sdl3
{
using namespace input;

class SDL3GameWindow : public Window
{
   public:
    SDL3GameWindow(std::string name, int width, int height);
    ~SDL3GameWindow();

    void Run(WindowApp&) override;

   private:
    void PollEvents();
    WindowHandle GetCurrentWindow();

    SDL_Window* m_window;
    Timer m_timer;
    bool m_shouldClose = false;

    float window_width;
    float window_height;

    struct Impl
    {
    #ifdef PLATFORM_DARWIN
        SDL_MetalView metalView = nullptr;
        void* metalLayer = nullptr;
    #endif
    };

    Impl m_hidden = {};
};

}  // namespace oge::platform::sdl3
