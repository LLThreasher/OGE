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
using oge::runtime::ui::UIDrag;
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

    // --- AABB wire cubes (3D) ---
    // Draw wireframe cubes around entities with AABB colliders.
    for (auto [entity, collider] :
         game.view<ComponentAABBCollider>().each())
    {
        auto& aabb = collider.aabb;
        Point3 center{
            (aabb.min.x + aabb.max.x) * 0.5f,
            (aabb.min.y + aabb.max.y) * 0.5f,
            (aabb.min.z + aabb.max.z) * 0.5f,
        };
        float extent = std::max(
            {aabb.max.x - aabb.min.x, aabb.max.y - aabb.min.y,
             aabb.max.z - aabb.min.z}) *
                       0.5f;

        f.submissionQueue.Add<CmdDrawWireCube>(
            GameViewType::Overlay,
            CmdDrawWireCube{center, extent, colors::BLUE});
    }

    // --- Creature wire cubes (3D) ---
    // Draw wireframe cubes around entities with Creature component.
    for (auto [entity, creature] :
         game.view<ComponentCreature>().each())
    {
        f.submissionQueue.Add<CmdDrawWireCube>(
            GameViewType::Overlay,
            CmdDrawWireCube{Point3{0, 0, 0}, 1.0f, colors::GREEN});
    }
}
}  // namespace game::view
