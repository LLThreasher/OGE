#pragma once

#include <optional>

#include "game/view/submission_queue.hpp"
#include "oge/runtime/gfx/commands.hpp"
#include "oge/runtime/gfx/draw_context.hpp"
#include "oge/runtime/typed_registry.hpp"
#include "oge/graphics/command_list.hpp"

namespace game::view::gfx
{
using namespace oge::runtime::gfx;

template <typename TQueue, typename... Passes>
class ViewExecutor
{
   public:
    ViewExecutor(TQueue& queue) : m_queue(queue) {}
    void Attach(OGEContextReadOnly& ctx)
    {
        m_ctx.emplace(ctx);
        std::apply([&](Passes&... args) { ((args.onAttach(m_ctx.value())), ...); }, m_passes);
    }

    void Detach()
    {
        std::apply([&](Passes&... args) { ((args.onDetach(m_ctx.value())), ...); }, m_passes);
        m_ctx.reset();
    }

    void Update(float dt)
    {
        DrawContext ctx(dt, m_ctx.value());
        m_queue.template Add<CmdAddView>(GameViewType::Overlay, IRect16{{0, 0}, ctx.backend.SwapchainExtent()});
        for (auto view : ALL_GAME_VIEWS)
        {
            DrawView(ctx, m_queue.GetSingle(view));
        }
        {
            DrawView(ctx, m_queue.GetSingle(GameViewType::Overlay));
        }
    }

   private:
    template <typename TView>
    void DrawView(DrawContext& ctx, TView queue)
    {
        auto& views = queue.template Get<CmdAddView>();
        if (views.empty()) return;
        auto cmdview = views[0];
        math::mat4 proj =
            math::get_perspective_rot(ctx.backend.SwapchainPretransform()) *
            (math::perspective_rev_z(cmdview.fov,
                                     cmdview.aspect == 0.f ? ctx.backend.SwapchainAspect() : cmdview.aspect, 0.1f));
        auto pvTransform = proj * cmdview.view;

        auto& rect = cmdview.rect;
        ctx.drawCmd.SetViewRect(rect.pos.x, rect.pos.y, rect.extent.x, rect.extent.y);

        std::apply(
            [&](Passes&... args)
            {
                ((
                     [&](auto&& value)
                     {
                         using T = std::decay_t<decltype(value)>;

                         if constexpr (std::derived_from<T, RequiresVPTransform>)
                             value.onUpdate(ctx, value.ExtractView(queue), pvTransform);
                         else
                             value.onUpdate(ctx, value.ExtractView(queue));
                     }(args)),
                 ...);
            },
            m_passes);
    }

    TQueue& m_queue;
    std::optional<InitDrawContext> m_ctx;
    std::tuple<Passes...> m_passes;
};
}  // namespace game::view::gfx
