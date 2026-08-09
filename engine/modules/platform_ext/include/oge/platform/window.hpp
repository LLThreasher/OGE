#pragma once

#include <functional>
#include <memory>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "oge/platform/window_app.hpp"

namespace oge::platform
{
class Window
{
   public:
    virtual ~Window() = default;
    virtual void Run(WindowApp& app) = 0;
};

using WindowFactory = std::unique_ptr<Window> (*)(std::string_view title, uint64_t width,
                                          uint64_t height);

void RegisterWindowFactory(std::string_view name,
                           WindowFactory factory);

std::unique_ptr<Window> CreateWindow(std::string_view backend,
                                    std::string_view title, uint64_t width,
                                    uint64_t height);

}  // namespace oge::platform
