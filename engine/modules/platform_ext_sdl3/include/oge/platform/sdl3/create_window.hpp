#pragma once
#include <memory>
#include <string_view>

#include "oge/platform/window.hpp"

namespace oge::platform
{
class Window;
}  // namespace oge::platform

namespace oge::platform::sdl3
{
std::unique_ptr<Window> CreateSDL3Window(std::string_view title, uint64_t width,
                                         uint64_t height);
}  // namespace oge::platform::sdl3
