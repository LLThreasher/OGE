#include "game/view/renderer.hpp"
#include "game/ui/objects.hpp"
#include "game/events.hpp"
#include "game/view/submission_queue.hpp"
#include "oge/point2.hpp"
#include "oge/runtime/gfx/commands.hpp"
#include "oge/graphics/backend.hpp"

namespace game::view
{
using oge::I16Point2;
using oge::U16Point2;
using namespace ui;

static void onCreateUIRect(entt::registry& gameWorld, entt::entity entity)
{
    gameWorld.emplace_or_replace<ScreenRect>(entity, UIRectToScreenRect(gameWorld, entity));
    if (!gameWorld.all_of<UIParent>(entity))
    {
        gameWorld.emplace<UIParent>(entity, gameWorld.view<UIRoot>().front());
    }
}

static void onDestroyUIRect(entt::registry& gameWorld, entt::entity entity)
{
    for (auto [e, p] : gameWorld.view<UIParent>().each())
    {
        if (p.parent == entity)
        {
            gameWorld.destroy(e);
        }
    }
}

static void onSurfaceRecreate(entt::registry& world, SurfaceRecreateEvent& event)
{
    world.clear<ScreenRect>();
    auto root = world.view<UIRoot>().front();
    world.emplace<ScreenRect>(root, ScreenRect{I16Point2{0, 0}, event.swapchainExtent});
    for (auto e : world.view<UIRect>())
    {
        onCreateUIRect(world, e);
    }
}

void UIRenderer::onAttach(RendererState& ctx)
{
    auto& game = ctx.world;
    game.on_construct<UIRect>().connect<&onCreateUIRect>();
    game.on_destroy<UIRect>().connect<&onDestroyUIRect>();
    ctx.events.sink<SurfaceRecreateEvent>().connect<&onSurfaceRecreate>(game);

    auto rootView = game.create();
    game.emplace<ScreenRect>(rootView, I16Point2{0, 0}, ctx.assets.backend.SwapchainExtent());
    game.emplace<UIRoot>(rootView);
}

void UIRenderer::onDetach(RendererState& ctx)
{
    auto& game = ctx.world;
    game.on_construct<UIRect>().disconnect<&onCreateUIRect>();
    game.on_destroy<UIRect>().disconnect<&onDestroyUIRect>();
    ctx.events.sink<SurfaceRecreateEvent>().disconnect<&onSurfaceRecreate>(game);
}

void UIRenderer::onUpdate(FRendererState& f)
{
    auto& game = f.world;
    for (auto [entity, rect] : game.view<UIRaycastTarget, ScreenRect>().each())
    {
        f.submissionQueue.Add<CmdDrawDebugRect>(GameViewType::Overlay,
                                             CmdDrawDebugRect{rect, game.all_of<UIDrag>(entity)         ? COLOR_GREEN
                                                                    : game.all_of<UIRaycastHit>(entity) ? COLOR_RED
                                                                                                        : COLOR_WHITE});
    }

    auto spQueue = f.submissionQueue.GetSingle(GameViewType::Overlay).View<oge::runtime::gfx::CmdDrawSprite>();
    for (auto [entity, uitext, rect] : game.view<UIText, ScreenRect>().each())
    {
        uitext.font->CreateTextSprites(spQueue, uitext.data, rect);
        // f.graphicQueue.Add<CmdDrawDebugRect>(GameViewType::Overlay, CmdDrawDebugRect{rect, COLOR_GREY});
    }

    for (auto [entity, uisp, rect] : game.view<UISprite, ScreenRect>().each())
    {
        f.submissionQueue.Add<CmdDrawSprite>(GameViewType::Overlay, CmdDrawSprite{rect, uisp.color, uisp.sprite});
        // f.graphicQueue.Add<CmdDrawDebugRect>(GameViewType::Overlay, CmdDrawDebugRect{rect, COLOR_GREY});
    }
}
}