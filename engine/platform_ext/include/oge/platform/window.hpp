#pragma once

#include <functional>

#include "oge/platform/window_app.hpp"

namespace oge::platform
{
    class Window
    {
        virtual void Run(WindowApp& app);
    };
} // namespace oge::platform
