#include "game/input/input_source.hpp"
#include "game/ui/objects.hpp"
#include "oge/fmt.hpp"
#include "oge/input/raw_input_stream.hpp"
#include "oge/log.hpp"
#include "oge/point2.hpp"
#include "oge/runtime/oge_registry.hpp"
#include "oge/runtime/ui/objects.hpp"

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

bool UIDrag::IsHold(const OgeRegistryRef world, int pixelRadiusSqr) const
{
    auto diff = ui::RelSpaceToScreenSpace(world, maxDragDelta);
    auto len = diff.x * diff.x + diff.y * diff.y;
    return len < pixelRadiusSqr;
}

bool UIDrag::IsClick(const OgeRegistryRef world, float duration,
                     int pixelRadiusSqr) const
{
    if (deltaTime > duration) return false;
    return IsHold(world, pixelRadiusSqr);
}

bool IsButtonClicked(const OgeRegistryRef game, entt::entity button)
{
    math::vec2 clickPos;
    return IsButtonClicked(game, button, clickPos);
}

bool IsButtonClicked(const OgeRegistryRef game, entt::entity button,
                     math::vec2& clickPos)
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

bool IsDragReleasedSrc(const oge::runtime::OgeRegistryRef game, entt::entity src)
{
    return game.all_of<UIDragReleaseFinished>(src);
}

std::tuple<const UIDrag*, entt::entity> TryGetReleasedDragSrc(
    const oge::runtime::OgeRegistryRef game, entt::entity e)
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

std::tuple<const UIDrag*, entt::entity> TryGetReleasedDragDst(
    const OgeRegistryRef game, entt::entity e)
{
    if (auto dragRel = game.try_get<UIDragReleaseDst>(e))
    {
        if (IsDragReleasedSrc(game, dragRel->dragStart))
        {
            return {game.try_get<UIDrag>(dragRel->dragStart),
                    dragRel->dragStart};
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
    for (auto e : game.view<UICursor>())
    {
        game.destroy(e);
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

    auto handlePtrDown = [&]()
    {
        if (!game.valid(activeDrags[ptrIdx]))
        {
            ptrPos = raw.PollPtrLatest(ptrIdx, raw_idx);
            hit = ui::CastRayRelSpace(game, ptrPos);
            if (game.valid(hit) && !game.all_of<UIDrag>(hit))
            {
                auto& drag = game.emplace<UIDrag>(hit);
                drag.inputIndex = ptrIdx;
                drag.dragStartPos = ptrPos;
                drag.dragLastPos = ptrPos;
                drag.onTopOf = hit;
                drag.dragStartButton = event.mouse.button();
                activeDrags[drag.inputIndex] = hit;
            }
        }
    };
    auto handleDragUpdate = [&]()
    {
        if (game.valid(activeDrags[ptrIdx]))
        {
            UIDrag& drag = game.get<UIDrag>(activeDrags[ptrIdx]);
            if (drag.dragStartButton == event.mouse.button())
            {
                ptrPos = raw.PollPtrLatest(event.mouse.ptrIdx(), raw_idx);
                hit = ui::CastRayRelSpace(game, ptrPos);
                drag.UpdateDrag(ptrPos, hit, ctx.dt);
                if (game.valid(hit) && !game.all_of<UIDragReleaseDst>(hit))
                    game.emplace<UIDragReleaseDst>(hit, activeDrags[ptrIdx]);
                game.emplace<UIDragReleaseFinished>(activeDrags[ptrIdx], hit);
                activeDrags[ptrIdx] = entt::null;
            }
        }
    };

    while (raw.PollEvent(raw_idx, event))
    {
        switch (event.type)
        {
            case InputEventType::MouseButtonDown:
                ptrIdx = event.mouse.ptrIdx();
                handlePtrDown();
                break;
            case InputEventType::MouseButtonUp:
                ptrIdx = event.mouse.ptrIdx();
                handleDragUpdate();
                break;
            case InputEventType::PointerDown:
                ptrIdx = event.pointerIdx;
                handlePtrDown();
                break;
            case InputEventType::PointerUp:
                ptrIdx = event.pointerIdx;
                handleDragUpdate();
                break;
            default:
                break;
        }
    }

    for (size_t ptrIdx : raw.ActivePtrs())
    {
        if (!game.valid(activeDrags[ptrIdx])) continue;
        ptrPos = raw.PollPtrLatest(ptrIdx, raw_idx);
        hit = ui::CastRayRelSpace(game, ptrPos);

        game.get<UIDrag>(activeDrags[ptrIdx]).UpdateDrag(ptrPos, hit, ctx.dt);
        if (game.valid(hit) && !game.all_of<UIDragReleaseDst>(hit))
        {
            game.emplace<UIDragReleaseDst>(hit, activeDrags[ptrIdx]);
        }
    }
}
}  // namespace game::input
