#include "game/input/input_source.hpp"

#include <cstdint>

#include "game/input/player_input_stream.hpp"
#include "game/ui/objects.hpp"
#include "oge/input/keyboard.hpp"
#include "oge/input/mouse.hpp"
#include "oge/input/raw_input_stream.hpp"
#include "oge/log.hpp"
#include "oge/math.hpp"
#include "oge/fmt.hpp"

namespace game::input
{

void KeyMouseInput::onAttach(InputContext& ctx) { ctx.windowCtx.SetMouseVisible(false); }

void KeyMouseInput::onDetach(InputContext& ctx) { ctx.windowCtx.SetMouseVisible(true); }

void KeyMouseInput::onUpdate(FInputContext& ctx)
{
    using namespace oge::input;
    auto& raw = ctx.raw;
    auto& keys = raw.ActiveKeys();

    math::vec2 moveDelta{
        (keys.get(KeyCode::KY_A) ? 1.f : 0.f) - (keys.get(KeyCode::KY_D) ? 1.f : 0.f),
        (keys.get(KeyCode::KY_W) ? 1.f : 0.f) - (keys.get(KeyCode::KY_S) ? 1.f : 0.f),
    };
    out.InsertMoveDelta(moveDelta);
    math::vec2 panDelta = raw.PollPtrDelta(mouseIdx, raw_idx) * math::vec2{hfov, vfov};
    out.InsertPanDelta(panDelta);

    PlayerInputEvent pEvent{{0.5f, 0.5f}};
    pEvent.actionMask = out.LatestAction();
    oge::input::InputEvent event{};
    while (raw.PollEvent(raw_idx, event))
    {
        if (event.type == InputEventType::MouseButtonDown)
        {
            if (event.mouse.button() == MouseButton::Left)
                pEvent.set<PlayerAction::Digging>();
            else if (event.mouse.button() == MouseButton::Right)
                pEvent.set<PlayerAction::Placing>();
        }
        else if (event.type == InputEventType::MouseButtonUp)
        {
            if (event.mouse.button() == MouseButton::Left)
                pEvent.unset<PlayerAction::Digging>();
            else if (event.mouse.button() == MouseButton::Right)
                pEvent.unset<PlayerAction::Placing>();
        }
        else if (event.type == InputEventType::KeyDown)
        {
            pEvent.set<PlayerAction::Jump>();
        }
    }
    out.InsertAction(pEvent);
}

void WidgetInput::onAttach(InputContext& ctx) { ctx.windowCtx.SetMouseVisible(false); }

void WidgetInput::onDetach(InputContext& ctx) { ctx.windowCtx.SetMouseVisible(true); }

void WidgetInput::onUpdate(FInputContext& ctx)
{
    using namespace ::game::ui;
    auto& game = ctx.uiWorld;
    // handle move
    {
        auto drag = game.try_get<UIDrag>(moveWidget);
        if (drag != nullptr)
        {
            math::vec2 moveDelta = drag->dragLastPos - drag->dragStartPos;
            moveDelta = -moveDelta;
            auto widgetSize = game.get<UIRect>(moveWidget).extent;
            moveDelta /= widgetSize * 0.5f;
            if (math::len_sq(moveDelta) > 1.f) moveDelta = math::normalize(moveDelta);
            out.InsertMoveDelta(moveDelta);
        }
    }
    // handle pan
    {
        PlayerInputEvent pEvent{};
        pEvent.actionMask = out.LatestAction() & (1 << static_cast<uint32_t>(PlayerAction::Digging));
        auto drag = game.try_get<UIDrag>(viewWidget);
        if (!isDigging)
        {
            if (drag != nullptr)
            {
                if (drag->deltaTime > 0.5f && drag->IsHold(game))
                {
                    isDigging = true;
                    pEvent.set<PlayerAction::Digging>();
                    pEvent.actionPos = drag->dragLastPos;
                }
                else
                {
                    out.InsertPanDelta(math::vec2{drag->dragDelta * math::vec2{hfov, vfov}} * 2.f);
                }
            }
        }
        else
        {
            pEvent.set<PlayerAction::Digging>();
            pEvent.actionPos = drag->dragLastPos;
        }

        auto [dragRel, _] = ui::TryGetReleasedDragSrc(game, viewWidget);
        if (dragRel != nullptr)
        {
            if (isDigging)
            {
                isDigging = false;
                pEvent.unset<PlayerAction::Digging>();
            }
            else if (dragRel->IsClick(game))
            {
                out.InsertAction({dragRel->dragLastPos, PlayerAction::Placing});
            }
        }
        out.InsertAction(pEvent);
    }
}

}  // namespace game::input
