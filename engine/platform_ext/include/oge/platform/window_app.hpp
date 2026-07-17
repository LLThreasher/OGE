#pragma once

#include <cinttypes>

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

inline AppFrameAction operator&(AppFrameAction a, AppFrameAction b)
{
    return static_cast<AppFrameAction>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}

inline AppFrameAction operator|(AppFrameAction a, AppFrameAction b)
{
    return static_cast<AppFrameAction>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

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
