#include "Engine/ECS/Components.hpp"
#include "Engine/ECS/GraphicalComponents.hpp"
#include "Engine/ECS/IRenderer.hpp"
#include "Engine/Graphics/SubmissionQueue.hpp"

namespace OneGame::Engine::ECS
{
void CameraRenderer::Initialize(GameWorldContext& game, PresentationContext& ctx)
{
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

void CameraRenderer::Present(const GameWorldContext& game, PresentationContext& ctx, FrameOutputData& fd)
{
    using namespace Graphics;
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
        fd.graphicQueue.Add<CmdAddView>(view.activeSlot, cmdview);
    }
}
}  // namespace OneGame::Engine::ECS