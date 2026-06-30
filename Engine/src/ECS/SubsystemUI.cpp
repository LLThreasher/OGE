#include "Engine/ECS/ISubsystem.hpp"
#include "Engine/Graphics/PresentationObjects.hpp"
#include "Engine/Math.hpp"

namespace OneGame::Engine::ECS
{
static entt::entity RunUIRaycast(entt::registry& gameWorld, math::vec2 pos)
{
    entt::entity resultEntity = entt::null;
    int maxZLevel = -1;
    for (auto [entity, rect] : gameWorld.view<UIRaycastTarget, const UIRect>().each())
    {
        if (rect.pos.x < pos.x && rect.pos.y < pos.y && pos.x < rect.pos.x + rect.extent.x &&
            pos.y < rect.pos.y + rect.extent.y)
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

void SubsystemUI::Initialize(GameWorldContext& game, AppContext ctx)
{
    for (auto& entity : activeDrags)
    {
        entity = entt::null;
    }
    game.world.on_construct<UIRect>().connect<entt::invoke<&SubsystemUI::onCreateUIRect>>();
}

void SubsystemUI::onCreateUIRect(entt::registry& gameWorld, entt::entity entity) {}

void SubsystemUI::Update(GameWorldContext& game, AppContext ctx, const FrameInputData& f)
{
    game.world.clear<UIDragRelease>();
    game.world.clear<UIRaycastHit>();
    // handle mouse drag
    math::vec2 mousePos{f.input.GetMouseX(), f.input.GetMouseY()};
    auto mouseEntityHit = RunUIRaycast(game.world, mousePos);
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
            e = RunUIRaycast(game.world, pos);
            game.world.emplace<UIDrag>(e, ptrIdx, MouseButton::Left, pos);
        }
        if (releasedTouchMask & (1 << pidx))
        {
            math::vec2 pos{f.input.GetTouchX(pidx), f.input.GetTouchY(pidx)};
            entt::entity& e = activeDrags[ptrIdx];
            assert(e != entt::null);
            auto resultE = RunUIRaycast(game.world, pos);
            game.world.emplace<UIDragRelease>(resultE, game.world.get<UIDrag>(e), e);
            game.world.erase<UIDrag>(e);
            e = entt::null;
        }
        if (activeDrags[ptrIdx] != entt::null)
        {
            math::vec2 pos{f.input.GetTouchX(pidx), f.input.GetTouchY(pidx)};
            auto hit = RunUIRaycast(game.world, pos);
            game.world.emplace<UIRaycastHit>(hit, activeDrags[ptrIdx]);
        }
    }
}

void SubsystemUI::Present(const GameWorldContext& game, PresentationContext ctx, FrameOutputData& f) {}
}  // namespace OneGame::Engine::ECS
