#include "Engine/ECS/ISubsystem.hpp"
#include "Engine/Graphics/PresentationObjects.hpp"
#include "Engine/Math.hpp"
#include "Engine/Point2.hpp"

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

entt::entity CastRayRelSpace(entt::registry& gameWorld, math::vec2 pos)
{
    return CastRayScreenSpace(gameWorld, RelSpaceToScreenSpace(gameWorld, pos));
}

entt::entity CastRayScreenSpace(entt::registry& gameWorld, math::vec2 pos)
{
    entt::entity resultEntity = entt::null;
    int maxZLevel = -1;
    for (auto [entity, rect, zLevel, srect] : gameWorld.view<UIRaycastTarget, const UIRect, const UIZLevel, const ScreenRect>().each())
    {
        if (srect.pos.x < pos.x && srect.pos.y < pos.y && pos.x < srect.pos.x + srect.extent.x &&
            pos.y < srect.pos.y + srect.extent.y)
        {
            if (zLevel.zLevel > maxZLevel)
            {
                maxZLevel = zLevel.zLevel;
                resultEntity = entity;
            }
        }
    }
    return resultEntity;
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
            srect.pos.x = (int32_t)(_rect->pos.x * srect.extent.x + srect.pos.x);
            srect.pos.y = (int32_t)(_rect->pos.y * srect.extent.y + srect.pos.y);
            srect.extent.x = (int32_t)(_rect->extent.x * srect.extent.x);
            srect.extent.y = (int32_t)(_rect->extent.y * srect.extent.y);
        }
        return srect;
    }
}

math::vec2 ScreenSpaceToRelSpace(const ScreenRect rect, math::vec2 screenPos)
{
    return (screenPos - static_cast<math::vec2>(rect.pos)) / static_cast<math::vec2>(rect.extent);
}

math::vec2 ScreenSpaceToRelSpace(entt::registry& world, entt::entity rectEntity, math::vec2 screenPos)
{
    auto rect = UIRectToScreenRect(world, rectEntity);
    return ScreenSpaceToRelSpace(rect, screenPos);
}

math::vec2 ScreenSpaceToRelSpace(entt::registry& world, math::vec2 screenPos)
{
    auto rectE = world.view<UIRoot>().front();
    return ScreenSpaceToRelSpace(world, rectE, screenPos);
}

Point2 RelSpaceToScreenSpace(entt::registry& world, math::vec2 relPos)
{
    auto root = world.view<UIRoot>().front();
    auto rect = UIRectToScreenRect(world, root);
    return Point2::FromVec2(relPos * static_cast<math::vec2>(rect.extent) + static_cast<math::vec2>(rect.pos));
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
    auto mouseEntityHit = game.world.view<InputSourceKeyMouse>().empty() ? UI::CastRayScreenSpace(game.world, mousePos) : entt::null;
    // mouseEntityHit = game.world.valid(mouseEntityHit) ? mouseEntityHit : game.world.view<UIRoot>().front();
    if (activeDrags[PointerIdx::MOUSE] == entt::null)
    {
        for (auto button : ALL_MOUSE_BUTTONS)
        {
            if (f.input.IsMousePressed(button) && game.world.valid(mouseEntityHit) && !game.world.all_of<UIDrag>(mouseEntityHit))
            {
                auto relMousePos = UI::ScreenSpaceToRelSpace(game.world, mousePos);
                auto& drag = game.world.emplace<UIDrag>(mouseEntityHit);
                drag.inputIndex = PointerIdx::MOUSE;
                drag.dragStartPos = relMousePos;
                drag.dragLastPos = relMousePos;
                drag.onTopOf = mouseEntityHit;
                drag.dragStartButton = button;
                activeDrags[PointerIdx::MOUSE] = mouseEntityHit;
                break;
            }
        }
    }
    else
    {
        UIDrag& mouseDrag = game.world.get<UIDrag>(activeDrags[PointerIdx::MOUSE]);
        mouseDrag.UpdateDrag(UI::ScreenSpaceToRelSpace(game.world, mousePos), mouseEntityHit, f.dt);
        if (f.input.IsMouseReleased(mouseDrag.dragStartButton))
        {
            if (game.world.valid(mouseEntityHit))
                game.world.emplace<UIDragRelease>(mouseEntityHit, mouseDrag, activeDrags[PointerIdx::MOUSE]);
            game.world.erase<UIDrag>(activeDrags[PointerIdx::MOUSE]);
            activeDrags[PointerIdx::MOUSE] = entt::null;
        }
    }
    if (game.world.valid(mouseEntityHit))
        game.world.get_or_emplace<UIRaycastHit>(mouseEntityHit);

    // handle touch drag
    uint32_t pressedTouchMask = f.input.GetPressedTouchIdMask();
    uint32_t releasedTouchMask = f.input.GetReleasedTouchIdMask();
    for (int pidx = 0; pidx < InputSystem::MaxTouches; ++pidx)
    {
        int ptrIdx = PointerIdx::PtrIdxFromTouchIdx(pidx);
        if (game.world.valid(activeDrags[ptrIdx]))
        {
            //LOG_DEBUG("Touch {} drag at ({}, {})", pidx, f.input.GetTouchX(pidx), f.input.GetTouchY(pidx));
            math::vec2 pos{f.input.GetTouchX(pidx), f.input.GetTouchY(pidx)};
            entt::entity hit = UI::CastRayRelSpace(game.world, pos);
            game.world.get<UIDrag>(activeDrags[ptrIdx]).UpdateDrag(pos, hit, f.dt);
            if (game.world.valid(hit)) game.world.get_or_emplace<UIRaycastHit>(hit);
        }
        else if (pressedTouchMask & (1 << pidx))
        {
            math::vec2 pos{f.input.GetTouchX(pidx), f.input.GetTouchY(pidx)};
            entt::entity hit = UI::CastRayRelSpace(game.world, pos);
            entt::entity edrag = activeDrags[ptrIdx];

            if (game.world.valid(hit) && !game.world.all_of<UIDrag>(hit))
            {
                //LOG_DEBUG("Touch {} drag start at ({}, {})", pidx, f.input.GetTouchX(pidx), f.input.GetTouchY(pidx));
                auto& drag = game.world.emplace<UIDrag>(hit);
                drag.inputIndex = ptrIdx;
                drag.dragStartPos = pos;
                drag.dragLastPos = pos;
                drag.onTopOf = hit;
                activeDrags[ptrIdx] = hit;
                game.world.get_or_emplace<UIRaycastHit>(hit);
            }
        }
        if (releasedTouchMask & (1 << pidx) && game.world.valid(activeDrags[ptrIdx]))
        {
            //LOG_DEBUG("Touch {} released at ({}, {})", pidx, f.input.GetTouchX(pidx), f.input.GetTouchY(pidx));
            math::vec2 pos{f.input.GetTouchX(pidx), f.input.GetTouchY(pidx)};
            entt::entity& e = activeDrags[ptrIdx];
            auto resultE = UI::CastRayRelSpace(game.world, pos);
            auto& drag = game.world.get<UIDrag>(e);
            if (game.world.valid(resultE))
                game.world.emplace<UIDragRelease>(resultE, drag, e);
            game.world.erase<UIDrag>(e);
            e = entt::null;
        }
    }
}

void SubsystemUI::Present(const GameWorldContext& game, PresentationContext ctx, FrameOutputData& f)
{
    for (auto [entity, rect] : game.world.view<UIRaycastTarget, ScreenRect>().each())
    {
        auto r = f.presentationWorld.create();
        f.presentationWorld.emplace<Graphics::PDebugRect>(r, rect.pos, rect.extent, game.world.all_of<UIDrag>(entity) ? COLOR_GREEN : game.world.all_of<UIRaycastHit>(entity) ? COLOR_RED : COLOR_WHITE);
    }
}
}  // namespace OneGame::Engine::ECS
