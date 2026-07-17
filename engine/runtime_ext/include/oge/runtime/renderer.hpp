#pragma once

#include "oge/runtime/staged_scheduler.hpp"

namespace oge::runtime
{
    template <typename Impl>
    class Renderer : FrameUpdateStage<Impl, RendererContext>
    {
    };
} // namespace oge::runtime
