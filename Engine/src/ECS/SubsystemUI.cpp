#include "Engine/ECS/ISubsystem.hpp"
#include "Engine/Graphics/PresentationObjects.hpp"
#include "Engine/Math.hpp"

namespace OneGame::Engine::UI
{
using namespace ECS;

entt::entity CreateGameView(entt::registry& game, const UIRect rect)
{
    std::unordered_set freeSlots{
        Graphics::GameViewType::Slot0,
        Graphics::GameViewType::Slot1,
        Graphics::GameViewType::Slot2,
        Graphics::GameViewType::Slot3,
    };
    for (auto [entity, view] : game.view<ViewPanel>().each())
    {
        freeSlots.erase(view.activeSlot);
    }
    if (freeSlots.empty())
    {
        assert(false && "too much view panels");
    }
    auto res = game.create();
    game.emplace<ViewPanel>(res).activeSlot = *freeSlots.begin();
    game.emplace<UIRect>(res, rect);
    return res;
}

entt::entity CastRay(entt::registry& gameWorld, math::vec2 pos)
{
    entt::entity resultEntity = entt::null;
    int maxZLevel = -1;
    for (auto [entity, rect, srect] : gameWorld.view<UIRaycastTarget, const UIRect, const ScreenRect>().each())
    {
        if (srect.pos.x < pos.x && srect.pos.y < pos.y && pos.x < srect.pos.x + srect.extent.x &&
            pos.y < srect.pos.y + srect.extent.y)
        {
            if (rect.zLevel > maxZLevel)
            {
                maxZLevel = rect.zLevel;
                resultEntity = entity;
            }
        }
    }
    return resultEntity;
}

math::vec2 ScreenSpaceToRelSpace(const ScreenRect rect, math::vec2 screenPos)
{
    return (screenPos - static_cast<math::vec2>(rect.pos)) / static_cast<math::vec2>(rect.extent);
}

ScreenRect UIRectToScreenRect(entt::registry& world, entt::entity rect)
{
    if (auto sr = world.try_get<ScreenRect>(rect))
    {
        return *sr;
    }
    else
    {
        auto parent = world.try_get<UIParent>(rect);
        auto parentEntity = parent ? parent->parent : world.view<UIRoot>().front();
        ScreenRect srect = UIRectToScreenRect(world, parentEntity);
        if (auto _rect = world.try_get<UIRect>(rect))
        {
            srect.pos.x = (int32_t)(_rect->pos.x * srect.pos.x);
            srect.pos.y = (int32_t)(_rect->pos.y * srect.pos.y);
            srect.extent.x = (int32_t)(_rect->extent.x * srect.extent.x);
            srect.extent.y = (int32_t)(_rect->extent.y * srect.extent.y);
        }
        return srect;
    }
}

math::vec2 ScreenSpaceToRelSpace(entt::registry& world, entt::entity rectEntity, math::vec2 screenPos)
{
    auto rect = UIRectToScreenRect(world, rectEntity);
    return (screenPos - static_cast<math::vec2>(rect.pos)) / static_cast<math::vec2>(rect.extent);
}
}

namespace OneGame::Engine::ECS
{

static void onCreateUIRect(entt::registry& gameWorld, entt::entity entity)
{
    gameWorld.emplace_or_replace<ScreenRect>(entity, UI::UIRectToScreenRect(gameWorld, entity));
    if (!gameWorld.all_of<UIParent>(entity))
    {
        gameWorld.emplace<UIParent>(entity, gameWorld.view<UIRoot>().front());
    }
}

static void onSurfaceRecreate(entt::registry& world, SurfaceRecreateEvent& event)
{
    world.clear<ScreenRect>();
    auto root = world.view<UIRoot>().front();
    world.emplace<ScreenRect>(root, Point2{0, 0}, event.swapchainExtent);
    for (auto e : world.view<UIRect>())
    {
        onCreateUIRect(world, e);
    }
}

void SubsystemUI::Initialize(GameWorldContext& game, AppContext ctx)
{
    for (auto& entity : activeDrags)
    {
        entity = entt::null;
    }
    auto rootView = game.world.create();
    game.world.emplace<UIRoot>(rootView);
    game.world.emplace<ScreenRect>(rootView, Point2{0, 0}, ctx.backend->SwapchainExtent());
    game.world.on_construct<UIRect>().connect<&onCreateUIRect>();
    ctx.events.sink<SurfaceRecreateEvent>().connect<&onSurfaceRecreate>(game.world);
}

void SubsystemUI::Update(GameWorldContext& game, AppContext ctx, const FrameInputData& f)
{
    game.world.clear<UIDragRelease>();
    game.world.clear<UIRaycastHit>();
    // handle mouse drag
    math::vec2 mousePos{f.input.GetMouseX(), f.input.GetMouseY()};
    auto mouseEntityHit = UI::CastRay(game.world, mousePos);
    if (activeDrags[PointerIdx::MOUSE] == entt::null)
    {
        for (auto button : ALL_MOUSE_BUTTONS)
        {
            if (f.input.IsMousePressed(button))
            {
                game.world.emplace<UIDrag>(mouseEntityHit, PointerIdx::MOUSE, button, mousePos);
                activeDrags[PointerIdx::MOUSE] = mouseEntityHit;
                break;
            }
        }
    }
    else
    {
        const UIDrag& mouseDrag = game.world.get<const UIDrag>(activeDrags[PointerIdx::MOUSE]);
        if (f.input.IsMouseReleased(mouseDrag.dragStartButton))
        {
            game.world.emplace<UIDragRelease>(mouseEntityHit, mouseDrag, activeDrags[PointerIdx::MOUSE]);
            game.world.erase<UIDrag>(activeDrags[PointerIdx::MOUSE]);
            activeDrags[PointerIdx::MOUSE] = entt::null;
        }
    }
    if (mouseEntityHit != entt::null) game.world.emplace<UIRaycastHit>(mouseEntityHit, activeDrags[PointerIdx::MOUSE]);

    // handle touch drag
    uint32_t pressedTouchMask = f.input.GetPressedTouchIdMask();
    uint32_t releasedTouchMask = f.input.GetReleasedTouchIdMask();
    for (int pidx = 0; pidx < InputSystem::MaxTouches; ++pidx)
    {
        int ptrIdx = PointerIdx::PtrIdxFromTouchIdx(pidx);
        if (pressedTouchMask & (1 << pidx))
        {
            math::vec2 pos{f.input.GetTouchX(pidx), f.input.GetTouchY(pidx)};
            entt::entity& e = activeDrags[ptrIdx];
            assert(e == entt::null);
            e = UI::CastRay(game.world, pos);
            game.world.emplace<UIDrag>(e, ptrIdx, MouseButton::Left, pos);
        }
        if (releasedTouchMask & (1 << pidx))
        {
            math::vec2 pos{f.input.GetTouchX(pidx), f.input.GetTouchY(pidx)};
            entt::entity& e = activeDrags[ptrIdx];
            assert(e != entt::null);
            auto resultE = UI::CastRay(game.world, pos);
            game.world.emplace<UIDragRelease>(resultE, game.world.get<UIDrag>(e), e);
            game.world.erase<UIDrag>(e);
            e = entt::null;
        }
        if (activeDrags[ptrIdx] != entt::null)
        {
            math::vec2 pos{f.input.GetTouchX(pidx), f.input.GetTouchY(pidx)};
            auto hit = UI::CastRay(game.world, pos);
            game.world.emplace<UIRaycastHit>(hit, activeDrags[ptrIdx]);
        }
    }
}

void SubsystemUI::Present(const GameWorldContext& game, PresentationContext ctx, FrameOutputData& f) {}
}  // namespace OneGame::Engine::ECS
