#include "game/components.hpp"
#include "game/view/gfx/gizmo_commands.hpp"
#include "game/view/renderer.hpp"
#include "game/view/submission_queue.hpp"
#include "oge/log.hpp"

namespace game::view
{

void BlockHighlightRenderer::onAttach(RendererState& ctx)
{
    LOG_DEBUG("block highlight renderer attached");
}

void BlockHighlightRenderer::onDetach(RendererState& ctx)
{
    LOG_DEBUG("block highlight renderer detached");
}

void BlockHighlightRenderer::onUpdate(FRendererState& f)
{
    auto& game = f.world;
    auto& ui = f.uiWorld;

    for (auto [e, viewPanel] : ui.view<ViewPanel>()->each())
    {
        auto camEntity = viewPanel.activeCamera;
        if (!game.valid(camEntity)) continue;

        auto target = game.try_get<ComponentTargetBlock>(camEntity);
        if (!target || !target->valid) continue;

        // Block AABB: integer block coords → unit cube
        AABB aabb{
            math::vec3(target->hitPos.x, target->hitPos.y, target->hitPos.z),
            math::vec3(target->hitPos.x + 1, target->hitPos.y + 1,
                       target->hitPos.z + 1)};

        f.submissionQueue.Add<CmdDrawWireCube>(viewPanel.activeSlot,
                                               CmdDrawWireCube{aabb, WHITE});
    }
}

}  // namespace game::view
