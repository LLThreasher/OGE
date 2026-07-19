#include "game/input/input_source.hpp"
#include "game/ui/objects.hpp"
#include "oge/input/raw_input_stream.hpp"

namespace game::ui
{
void UIDrag::UpdateDrag(math::vec2 pos, entt::entity onTopOf, float dt)
{
    dragDelta = pos - dragLastPos;
    dragLastPos = pos;
    onTopOf = onTopOf;
    deltaTime += dt;
    auto totalOffset = math::abs(dragLastPos - dragStartPos);
    maxDragDelta.x = math::max(maxDragDelta.x, totalOffset.x);
    maxDragDelta.y = math::max(maxDragDelta.y, totalOffset.y);
}

bool UIDrag::IsHold(const entt::registry& world, int pixelRadiusSqr) const
{
    auto diff = ui::RelSpaceToScreenSpace(world, maxDragDelta);
    auto len = diff.x * diff.x + diff.y * diff.y;
    return len < pixelRadiusSqr;
}

bool UIDrag::IsClick(const entt::registry& world, float duration, int pixelRadiusSqr) const
{
    if (deltaTime > duration) return false;
    return IsHold(world, pixelRadiusSqr);
}

bool IsButtonClicked(const entt::registry& game, entt::entity button)
{
    math::vec2 clickPos;
    return IsButtonClicked(game, button, clickPos);
}

bool IsButtonClicked(const entt::registry& game, entt::entity button, math::vec2& clickPos)
{
    if (auto drag = game.try_get<UIDrag>(button))
    {
        if (!IsDragReleasedSrc(game, button)) return false;
        if (drag->IsClick(game))
        {
            clickPos = drag->dragLastPos;
            return true;
        }
    }
    return false;
}

bool IsDragReleasedSrc(const entt::registry& game, entt::entity src)
{
    return game.all_of<UIDragReleaseFinished>(src);
}

std::tuple<const UIDrag*, entt::entity> TryGetReleasedDragSrc(const entt::registry& game, entt::entity e)
{
    if (auto drag = game.try_get<UIDrag>(e))
    {
        if (auto dragRelFin = game.try_get<UIDragReleaseFinished>(e))
        {
            return {drag, dragRelFin->dragDst};
        }
    }
    return {nullptr, entt::null};
}

std::tuple<const UIDrag*, entt::entity> TryGetReleasedDragDst(const entt::registry& game, entt::entity e)
{
    if (auto dragRel = game.try_get<UIDragReleaseDst>(e))
    {
        if (IsDragReleasedSrc(game, dragRel->dragStart))
        {
            return {game.try_get<UIDrag>(dragRel->dragStart), dragRel->dragStart};
        }
    }
    return {nullptr, entt::null};
}
}  // namespace game::ui

namespace game::input
{

using namespace ::game::ui;
void UIDragInput::onAttach(InputContext& ctx)
{
    for (auto& entity : activeDrags)
    {
        entity = entt::null;
    }
}

void UIDragInput::onDetach(InputContext& ctx)
{
}

void UIDragInput::onUpdate(FInputContext& ctx)
{
    auto& game = ctx.uiWorld;
    for (auto e : game.view<UIDragReleaseFinished>())
    {
        game.erase<UIDrag>(e);
    }
    game.clear<UIDragReleaseFinished>();
    game.clear<UIDragReleaseDst>();
    game.clear<UIRaycastHit>();

    auto& raw = ctx.raw;

    using oge::input::InputEventType;
    oge::input::InputEvent event{};
    entt::entity hit;
    math::vec2 ptrPos;
    size_t ptrIdx;

    while (raw.PollEvent(raw_idx, event))
    {
        switch (event.type)
        {
            case InputEventType::MouseButtonDown:
                ptrIdx = event.mouse.ptrIdx();
                if (!game.valid(activeDrags[ptrIdx]))
                {
                    ptrPos = raw.PollPtrLatest(ptrIdx, raw_idx);
                    hit = ui::CastRayScreenSpace(game, ptrPos);
                    if (game.valid(hit) && !game.all_of<UIDrag>(hit))
                    {
                        auto relMousePos = ui::ScreenSpaceToRelSpace(game, ptrPos);
                        auto& drag = game.emplace<UIDrag>(hit);
                        drag.inputIndex = ptrIdx;
                        drag.dragStartPos = relMousePos;
                        drag.dragLastPos = relMousePos;
                        drag.onTopOf = hit;
                        drag.dragStartButton = event.mouse.button();
                        activeDrags[drag.inputIndex] = hit;
                    }
                }
                break;
            case InputEventType::MouseButtonUp:
                ptrIdx = event.mouse.ptrIdx();
                if (game.valid(activeDrags[ptrIdx]))
                {
                    UIDrag& mouseDrag = game.get<UIDrag>(activeDrags[ptrIdx]);
                    if (mouseDrag.dragStartButton == event.mouse.button())
                    {
                        ptrPos = raw.PollPtrLatest(event.mouse.ptrIdx(), raw_idx);
                        hit = ui::CastRayScreenSpace(game, ptrPos);
                        mouseDrag.UpdateDrag(ui::ScreenSpaceToRelSpace(game, ptrPos), hit, ctx.dt);
                        if (game.valid(hit) && !game.all_of<UIDragReleaseDst>(hit))
                            game.emplace<UIDragReleaseDst>(hit, activeDrags[ptrIdx]);
                        game.emplace<UIDragReleaseFinished>(activeDrags[ptrIdx], hit);
                        activeDrags[ptrIdx] = entt::null;
                    }
                }
                break;
            case InputEventType::PointerDown:
                ptrIdx = event.pointerIdx;
                if (!game.valid(activeDrags[ptrIdx]))
                {
                    ptrPos = raw.PollPtrLatest(event.pointerIdx, raw_idx);
                    hit = ui::CastRayRelSpace(game, ptrPos);

                    if (game.valid(hit) && !game.all_of<UIDrag>(hit))
                    {
                        // LOG_DEBUG("Touch {} drag start at ({}, {})", pidx, f.input.GetTouchX(pidx),
                        // f.input.GetTouchY(pidx));
                        auto& drag = game.emplace<UIDrag>(hit);
                        drag.inputIndex = ptrIdx;
                        drag.dragStartPos = ptrPos;
                        drag.dragLastPos = ptrPos;
                        drag.onTopOf = hit;
                        activeDrags[ptrIdx] = hit;
                    }
                }
                break;
            case InputEventType::PointerUp:
                ptrIdx = event.pointerIdx;
                if (game.valid(activeDrags[ptrIdx]))
                {
                    ptrPos = raw.PollPtrLatest(event.pointerIdx, raw_idx);
                    hit = ui::CastRayRelSpace(game, ptrPos);

                    entt::entity& e = activeDrags[ptrIdx];
                    auto& drag = game.get<UIDrag>(e);
                    drag.UpdateDrag(ptrPos, hit, ctx.dt);
                    game.emplace<UIDragReleaseFinished>(e, hit);
                    e = entt::null;
                }
                break;
            default:
                break;
        }
    }

    for (size_t ptrIdx : raw.ActivePtrs())
    {
        if (!game.valid(activeDrags[ptrIdx])) continue;
        ptrPos = raw.PollPtrLatest(event.pointerIdx, raw_idx);
        if (raw.IsMouse(ptrIdx))
        {
            hit = ui::CastRayScreenSpace(game, ptrPos);
            ptrPos = ui::ScreenSpaceToRelSpace(game, ptrPos);
        }
        else
        {
            hit = ui::CastRayRelSpace(game, ptrPos);
        }

        // LOG_DEBUG("Touch {} drag at ({}, {})", pidx, f.input.GetTouchX(pidx), f.input.GetTouchY(pidx));
        game.get<UIDrag>(activeDrags[ptrIdx]).UpdateDrag(ptrPos, hit, ctx.dt);
        if (game.valid(hit) && !game.all_of<UIDragReleaseDst>(hit))
        {
            game.emplace<UIDragReleaseDst>(hit, activeDrags[ptrIdx]);
        }
    }
}
}  // namespace game::input
