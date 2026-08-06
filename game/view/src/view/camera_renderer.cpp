#include <cassert>

#include "game/components.hpp"
#include "game/ui/objects.hpp"
#include "game/view/renderer.hpp"
#include "oge/fmt.hpp"
#include "oge/log.hpp"
#include "oge/math.hpp"
#include "oge/pool.hpp"

namespace game::view
{
using namespace ui;
static void onCameraCreated(entt::registry& renderWorld,
                            entt::registry& gameWorld, entt::entity e)
{
    if (!renderWorld.valid(e))
    {
        auto re = renderWorld.create(e);
        assert(re == e);
    }
    renderWorld.emplace<ComponentCamera>(e, gameWorld.get<ComponentCamera>(e));
}

static math::vec3 DecayToZero(math::vec3 value, float dt,
                              float sharpness = 16.f)
{
    float alpha = 1.0f - std::exp(-sharpness * dt);
    return math::lerp(value, math::vec3{0.0f}, alpha);
}

void CameraRenderer::onAttach(RendererState& ctx)
{
    auto& game = ctx.uiWorld;
    game.on_construct<ScreenRect>().connect<&CameraRenderer::onViewPanelUpdate>(
        ctx.world);
    game.on_update<ScreenRect>().connect<&CameraRenderer::onViewPanelUpdate>(
        ctx.world);
    game.on_construct<ViewPanel>().connect<&CameraRenderer::onViewPanelUpdate>(
        ctx.world);
    game.on_update<ViewPanel>().connect<&CameraRenderer::onViewPanelUpdate>(
        ctx.world);
    // ctx.world.on_construct<ComponentCamera>().connect<&onCameraCreated>(
    //     ctx.renderWorld);
}

void CameraRenderer::onViewPanelUpdate(entt::registry& world,
                                       entt::registry& uiWorld,
                                       entt::entity entity)
{
    auto [vp, rect] = uiWorld.try_get<ViewPanel, ScreenRect>(entity);
    if (vp != nullptr && rect != nullptr)
    {
        auto camEntity = vp->activeCamera;
        if (auto pcam = world.try_get<ComponentPerspectiveCamera>(camEntity))
        {
            if (rect->extent.y > 0)
            {
                pcam->aspect = static_cast<float>(rect->extent.x) /
                               static_cast<float>(rect->extent.y);
            }
        }
    }
}

void CameraRenderer::onUpdate(FRendererState& ctx)
{
    for (auto [entity, view, rect] :
         ctx.uiWorld.view<ViewPanel, ScreenRect>().each())
    {
        CmdAddView cmdview{};
        cmdview.rect = rect;
        if (!ctx.world.valid(view.activeCamera)) continue;
        if (auto camera = ctx.world.try_get<ComponentCamera>(view.activeCamera))
        {
            // auto& rcam = ctx.renderWorld.get<ComponentCamera>(view.activeCamera);
            // rcam.position = math::lerp(rcam.position, camera->position, ctx.alpha);
            // rcam.forward = camera->forward;
            // cmdview.view = rcam.view();
            cmdview.view = camera->view();
        }
        if (auto pcamera = ctx.world.try_get<ComponentPerspectiveCamera>(
                view.activeCamera))
        {
            cmdview.fov = pcamera->fov;
            cmdview.aspect = pcamera->aspect;
        }
        ctx.submissionQueue.Add<CmdAddView>(view.activeSlot, cmdview);
    }
}

void CameraRenderer::onDetach(RendererState& ctx)
{
}
}  // namespace game::view
