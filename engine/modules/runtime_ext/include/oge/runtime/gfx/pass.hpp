#pragma once

#include "oge/color.hpp"
#include "oge/rect.hpp"
#include "oge/runtime/objects_ext.hpp"
#include "oge/submission_group.hpp"

namespace oge::runtime::gfx
{

template <typename... Args>
class Pass
{
   public:
    using View = SubmissionView<Args...>;
    template <typename TQueue>
    TQueue::template TView<Args...> ExtractView(TQueue& queue)
    {
        return queue.template View<Args...>();
    }
};
}  // namespace oge::runtime::gfx