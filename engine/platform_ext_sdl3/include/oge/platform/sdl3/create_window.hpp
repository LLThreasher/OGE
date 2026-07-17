#pragma once
#include <memory>
#include "oge/platform/window.hpp"

namespace oge::platform
{
    class Window;
} // namespace oge::platform

namespace oge::platform::sdl3
{
std::unique_ptr<Window> CreateSDL3Window(const std::string& title, int width, int height);
}  // namespace oge::platform::sdl3
