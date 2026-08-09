#pragma once

#include <cinttypes>
#include <functional>
#include <string_view>

#include "oge/flag_helper.hpp"
#include "oge/input/raw_input_stream.hpp"
#include "oge/platform/window_handle.hpp"

namespace oge::platform
{
enum class AppFrameAction : uint32_t
{
    None = 0,
    WaitSurface = 1,
    WrapMouse = 1 << 1,
    UnwrapMouse = 1 << 2,
};

using InputProvider =
    std::function<void(input::RawInputStream& input, bool blocking)>;

class WindowApp
{
   public:
    virtual ~WindowApp() = default;
    virtual void Initialize(WindowHandle& handle) = 0;
    virtual AppFrameAction Update(float dt, InputProvider pollInputs) = 0;
    virtual void Shutdown() = 0;

    virtual void OnWindowRecreate(WindowHandle&) = 0;
    virtual void OnResize(int width, int height) = 0;
};

using WindowAppFactory = std::unique_ptr<WindowApp> (*)();

void RegisterWindowAppFactory(std::string_view name,
                           WindowAppFactory factory);

std::unique_ptr<WindowApp> CreateWindowApp(std::string_view name);
}  // namespace oge::platform
