#pragma once

#include <SDL3/SDL.h>

#include "oge/input/raw_input_stream.hpp"
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

    void Run(WindowApp&) override;

   private:
    void PollEvents();
    WindowHandle GetCurrentWindow();

    SDL_Window* m_window;
    Timer m_timer;
    bool m_shouldClose = false;

    float window_width;
    float window_height;
};

}  // namespace oge::platform::sdl3
