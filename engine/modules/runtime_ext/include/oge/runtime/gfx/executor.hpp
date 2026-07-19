#pragma once

#include <optional>

#include "oge/runtime/gfx/commands.hpp"
#include "oge/runtime/gfx/draw_context.hpp"
#include "oge/runtime/typed_registry.hpp"

namespace oge::runtime::gfx
{
template <typename TQueue, typename... Passes>
class Executor
{
   public:
    void Attach(OGEContextReadOnly& ctx)
    {
        m_queue = ctx.Get<TQueue>();
        m_ctx.emplace(ctx);
        std::apply([](const Passes&... args) { ((args.onAttach(m_ctx.value())), ...); }, m_passes);
    }

    void Detach()
    {
        std::apply([](const Passes&... args) { ((args.onDetach(m_ctx.value())), ...); }, m_passes);
        m_ctx.reset();
    }

    void Update(float dt)
    {
        DrawContext ctx(dt, m_ctx.value());
        std::apply(
            [](const Passes&... args)
            {
                ((
                     [](auto&& value)
                     {
                         value.onUpdate(ctx, value.ExtractView(*m_queue));
                     }(args)),
                 ...);
            },
            m_passes);
    }

   private:
    TQueue* m_queue;
    std::optional<InitDrawContext> m_ctx;
    std::tuple<Passes...> m_passes;
};
}  // namespace oge::runtime::gfx
