#pragma once

#include <functional>

#include "oge/platform/window_app.hpp"

namespace oge::platform
{
class Window
{
   public:
    virtual ~Window() = default;
    virtual void Run(WindowApp& app) = 0;
};
}  // namespace oge::platform
