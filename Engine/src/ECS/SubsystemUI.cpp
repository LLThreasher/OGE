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
        if (rect.pos.x < pos.x && rect.pos.y < pos.y && pos.x < rect.pos.x + rect.extend.x &&
            pos.y < rect.pos.y + rect.extend.y)
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

void SubsystemUI::Initialize(AppContext& ctx, entt::registry& gameWorld)
{
    for (auto& entity : activeDrags)
    {
        entity = entt::null;
    }
    gameWorld.on_construct<UIRect>().connect<entt::invoke<&SubsystemUI::onCreateUIRect>>();
}

void SubsystemUI::onCreateUIRect(entt::registry& gameWorld, entt::entity entity) {}

void SubsystemUI::Update(AppContext& ctx, entt::registry& gameWorld, const FrameInputData& f)
{
    gameWorld.clear<UIDragRelease>();
    gameWorld.clear<UIRaycastHit>();
    // handle mouse drag
    math::vec2 mousePos{f.input.GetMouseX(), f.input.GetMouseY()};
    auto mouseEntityHit = RunUIRaycast(gameWorld, mousePos);
    if (activeDrags[PointerIdx::MOUSE] == entt::null)
    {
        for (auto button : ALL_MOUSE_BUTTONS)
        {
            if (f.input.IsMousePressed(button))
            {
                gameWorld.emplace<UIDrag>(mouseEntityHit, PointerIdx::MOUSE, button, mousePos);
                activeDrags[PointerIdx::MOUSE] = mouseEntityHit;
                break;
            }
        }
    }
    else
    {
        const UIDrag& mouseDrag = gameWorld.get<const UIDrag>(activeDrags[PointerIdx::MOUSE]);
        if (f.input.IsMouseReleased(mouseDrag.dragStartButton))
        {
            gameWorld.emplace<UIDragRelease>(mouseEntityHit, mouseDrag, activeDrags[PointerIdx::MOUSE]);
            gameWorld.erase<UIDrag>(activeDrags[PointerIdx::MOUSE]);
            activeDrags[PointerIdx::MOUSE] = entt::null;
        }
    }
    if (mouseEntityHit != entt::null) gameWorld.emplace<UIRaycastHit>(mouseEntityHit, activeDrags[PointerIdx::MOUSE]);

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
            e = RunUIRaycast(gameWorld, pos);
            gameWorld.emplace<UIDrag>(e, ptrIdx, MouseButton::Left, pos);
        }
        if (releasedTouchMask & (1 << pidx))
        {
            math::vec2 pos{f.input.GetTouchX(pidx), f.input.GetTouchY(pidx)};
            entt::entity& e = activeDrags[ptrIdx];
            assert(e != entt::null);
            auto resultE = RunUIRaycast(gameWorld, pos);
            gameWorld.emplace<UIDragRelease>(resultE, gameWorld.get<UIDrag>(e), e);
            gameWorld.erase<UIDrag>(e);
            e = entt::null;
        }
        if (activeDrags[ptrIdx] != entt::null)
        {
            math::vec2 pos{f.input.GetTouchX(pidx), f.input.GetTouchY(pidx)};
            auto hit = RunUIRaycast(gameWorld, pos);
            gameWorld.emplace<UIRaycastHit>(hit, activeDrags[ptrIdx]);
        }
    }
}

void SubsystemUI::Present(const entt::registry& gameWorld, PresentationContext& pctx, FrameOutputData& f) {}
}  // namespace OneGame::Engine::ECS
