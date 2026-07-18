#pragma once
#include "oge/platform/window_app.hpp"

namespace game
{
using namespace oge;
using oge::platform::AppFrameAction;
class WindowCtx
{
    AppFrameAction frameAction;

public:
    void SetMouseVisible(bool val)
    {
        frameAction |= (val ? AppFrameAction::UnwrapMouse : AppFrameAction::WrapMouse);
    }
};
}  // namespace game
