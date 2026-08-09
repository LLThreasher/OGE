#include "game/components.hpp"
#include "game/events.hpp"
#include "game/ui/objects.hpp"
#include "game/view/gfx/gizmo_commands.hpp"
#include "game/view/renderer.hpp"
#include "game/view/submission_queue.hpp"
#include "oge/log.hpp"

namespace game::view
{
using oge::runtime::ui::UIRaycastHit;
using oge::runtime::ui::UIRaycastTarget;
using namespace ui;

void GizmoRenderer::onAttach(RendererState& ctx)
{
    // GizmoRenderer observes the game world (for AABB/creature entities)
    // and the UI world (for raycast target rects).
    LOG_DEBUG("gizmo renderer attached");
}

void GizmoRenderer::onDetach(RendererState& ctx)
{
    LOG_DEBUG("gizmo renderer detached");
}

void GizmoRenderer::onUpdate(FRendererState& f)
{
    auto& game = f.world;
    auto& ui = f.uiWorld;

    // --- UI raycast target wire rects ---
    // Draw wireframe rects around UI elements that have raycast targets,
    // color-coded by interaction state.
    for (auto [entity, rect] : ui.view<UIRaycastTarget, ScreenRect>().each())
    {
        f.submissionQueue.Add<CmdDrawDebugRect>(
            GameViewType::Overlay,
            CmdDrawDebugRect{rect, ui.all_of<UIDrag>(entity)  ? GREEN
                                   : ui.all_of<UIRaycastHit>(entity)
                                       ? RED
                                       : WHITE});
    }

    if (!ui.Raw().owned<ViewPanel>()) return;

    // --- AABB wire cubes (3D) ---
    // Draw wireframe cubes around entities with AABB colliders.
    for (auto [e, viewPanel] : ui.view<ViewPanel>()->each())
    {
        for (auto [entity, collider, body] :
            game.view<ComponentAABBCollider, ComponentPhysicBody>().each())
        {
            f.submissionQueue.Add<CmdDrawWireCube>(
                viewPanel.activeSlot,
                CmdDrawWireCube{collider.aabb + body.pos, BLUE});
        }
    }
}
}  // namespace game::view
