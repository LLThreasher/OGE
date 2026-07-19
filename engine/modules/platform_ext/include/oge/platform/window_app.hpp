#pragma once

#include <cinttypes>

#include "oge/input/raw_input_stream.hpp"
#include "oge/platform/window_handle.hpp"
#include "oge/flag_helper.hpp"

namespace oge::platform
{
enum class AppFrameAction : uint32_t
{
    None = 0,
    WaitSurface = 1,
    WrapMouse = 1 << 1,
    UnwrapMouse = 1 << 2,
};

class WindowApp
{
   public:
    virtual void Initialize(WindowHandle* handle) = 0;
    virtual AppFrameAction Update(float dt, input::RawInputStream& input) = 0;
    virtual void Shutdown() = 0;

    virtual void OnWindowRecreate(WindowHandle*) = 0;
    virtual void OnResize(int width, int height) = 0;
};
}  // namespace oge::platform
