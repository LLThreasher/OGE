#include "game/components.hpp"
#include "game/ui/objects.hpp"
#include "game/view/renderer.hpp"

namespace game::view
{
using namespace ui;
void CameraRenderer::onAttach(RendererState& ctx)
{
    auto& game = ctx.world;
    game.on_construct<ScreenRect>().connect<&CameraRenderer::onViewPanelUpdate>(this);
    game.on_update<ScreenRect>().connect<&CameraRenderer::onViewPanelUpdate>(this);
    game.on_construct<ViewPanel>().connect<&CameraRenderer::onViewPanelUpdate>(this);
    game.on_update<ViewPanel>().connect<&CameraRenderer::onViewPanelUpdate>(this);
}

void CameraRenderer::onViewPanelUpdate(entt::registry& world, entt::entity entity)
{
    auto [vp, rect] = world.try_get<ViewPanel, ScreenRect>(entity);
    if (vp != nullptr && rect != nullptr)
    {
        auto camEntity = vp->activeCamera;
        if (auto pcam = world.try_get<ComponentPerspectiveCamera>(camEntity))
        {
            pcam->aspect = (float)rect->extent.x / (float)rect->extent.y;
        }
    }
}

void CameraRenderer::onUpdate(FRendererState& ctx)
{
    auto& game = ctx.world;
    for (auto [entity, view, rect] : game.view<ViewPanel, ScreenRect>().each())
    {
        CmdAddView cmdview{};
        cmdview.rect = rect;
        if (auto camera = game.try_get<ComponentCamera>(view.activeCamera)) cmdview.view = camera->view();
        if (auto pcamera = game.try_get<ComponentPerspectiveCamera>(view.activeCamera))
        {
            cmdview.fov = pcamera->fov;
            cmdview.aspect = pcamera->aspect;
        }
        ctx.submissionQueue.Add<CmdAddView>(view.activeSlot, cmdview);
    }
}

void CameraRenderer::onDetach(RendererState& ctx) {}
}  // namespace game::view
