#include "oge/platform/window.hpp"
#include "oge/platform/window_app.hpp"

namespace oge::platform
{

std::unordered_map<std::string, WindowFactory> g_windowFactories;

void RegisterWindowFactory(std::string_view name, WindowFactory factory)
{
    g_windowFactories.emplace(std::string(name), std::move(factory));
}

std::unique_ptr<Window> CreateWindow(std::string_view backend,
                                     std::string_view title, uint64_t width,
                                     uint64_t height)
{
    return g_windowFactories.at(std::string(backend))(title, width, height);
}

std::unordered_map<std::string, WindowAppFactory> g_appFactories;

void RegisterWindowAppFactory(std::string_view name, WindowAppFactory factory)
{
    g_appFactories.emplace(std::string(name), factory);
}

std::unique_ptr<WindowApp> CreateWindowApp(std::string_view backend)
{
    return g_appFactories.at(std::string(backend))();
}

}  // namespace oge::platform
