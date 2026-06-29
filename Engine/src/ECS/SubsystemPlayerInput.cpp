#include "Engine/ECS/ISubsystem.hpp"
#include "Engine/Graphics/PresentationObjects.hpp"
#include "Engine/Math.hpp"

namespace OneGame::Engine::ECS
{
entt::entity PlayerInputData::CreatePlayerViewPanel(entt::registry& gameWorld, UIRect& rect)
{
    auto res = gameWorld.create();
    gameWorld.emplace<PlayerInputData>(res);
    gameWorld.emplace<PlayerViewPanel>(res);
    gameWorld.emplace<UIRect>(res, rect);
    return res;
}

static math::vec2 ScreenSpaceToSufaceSpace(const UIRect rect, math::vec2 screenPos)
{
    return (screenPos - rect.pos) / rect.extend;
}

void SubsystemPlayerInput::Initialize(GameWorldContext& game, AppContext ctx)
{
    game.world.on_construct<UIFocus>().connect<&SubsystemPlayerInput::onUIGainFocus>(this);
    game.world.on_destroy<UIFocus>().connect<&SubsystemPlayerInput::onUILoseFocus>(this);
}

void SubsystemPlayerInput::onUIGainFocus(entt::registry& gameWorld, entt::entity entity)
{
    if (auto panel = gameWorld.try_get<PlayerViewPanel>(entity))
    {
        // connect player control here
        if (!playerInputUsingKeyMouse.has_value())
        {
            panel->source |= InputSource::KeyMouse;
            playerInputUsingKeyMouse = entity;
            LOG_DEBUG("set playerInputUsingKeyMouse");
        }
    }
}

void SubsystemPlayerInput::onUILoseFocus(entt::registry& gameWorld, entt::entity entity)
{
    if (auto panel = gameWorld.try_get<PlayerViewPanel>(entity))
    {
        if (playerInputUsingKeyMouse == entity)
        {
            panel->source &= ~InputSource::KeyMouse;
            playerInputUsingKeyMouse.reset();
        }
        auto& input = gameWorld.get<PlayerInputData>(entity);
        input.moveDelta = {0, 0};
        input.panDelta = {0, 0};
    }
}

void SubsystemPlayerInput::Update(GameWorldContext& game, AppContext ctx, const FrameInputData& f)
{
    for (auto [entity, prect, panel, data] :
         game.world.view<const UIRect, const PlayerViewPanel, PlayerInputData>().each())
    {
        // handle touch
        if (panel.source & InputSource::Widget)
        {
            // handle move
            {
                auto& rect = game.world.get<UIRect>(panel.widgetInput.moveWidget);
                auto drag = game.world.try_get<UIDrag>(panel.widgetInput.moveWidget);
                if (drag != nullptr)
                {
                    auto touchIdx = PointerIdx::TouchIdxFromPtrIdx(drag->inputIndex);
                    math::vec2 pos = {f.input.GetTouchX(touchIdx), f.input.GetTouchY(touchIdx)};
                    data.moveDelta = ScreenSpaceToSufaceSpace(rect, pos - drag->dragStartPos);
                }
            }
            // handle pan
            {
                auto& rect = game.world.get<UIRect>(panel.widgetInput.viewWidget);
                auto drag = game.world.try_get<UIDrag>(panel.widgetInput.viewWidget);
                if (drag != nullptr)
                {
                    auto touchIdx = PointerIdx::TouchIdxFromPtrIdx(drag->inputIndex);
                    math::vec2 posDelta = {f.input.GetTouchDX(touchIdx), f.input.GetTouchDY(touchIdx)};
                    data.panDelta = ScreenSpaceToSufaceSpace(rect, posDelta);
                }
            }
        }
        // handle keyboard
        if (panel.source & InputSource::KeyMouse)
        {
            data.panDelta = ScreenSpaceToSufaceSpace(prect, {f.input.GetMouseDX(), -f.input.GetMouseDY()});
            if (f.input.IsKeyDown(KeyCode::KY_W))
                data.moveDelta.y = 1.0f;
            else if (f.input.IsKeyDown(KeyCode::KY_S))
                data.moveDelta.y = -1.0f;
            else
                data.moveDelta.y = 0.f;
            if (f.input.IsKeyDown(KeyCode::KY_A))
                data.moveDelta.x = 1.0f;
            else if (f.input.IsKeyDown(KeyCode::KY_D))
                data.moveDelta.x = -1.0f;
            else
                data.moveDelta.x = 0.f;
            data.moveDelta *= f.dt;
            data.moveDelta *= 5.f;
        }
    }
}

void SubsystemPlayerInput::Present(const GameWorldContext& game, PresentationContext ctx, FrameOutputData& f)
{
    if (playerInputUsingKeyMouse.has_value() && !isKeyMouseUsed)
    {
        SceneAction a{};
        a.type = SceneActionType::SetMouseWarpping;
        a.setMouseWarpping.enabled = true;
        f.outSceneActions.emplace_back(a);
        isKeyMouseUsed = true;
    }
    if (!playerInputUsingKeyMouse.has_value() && isKeyMouseUsed)
    {
        SceneAction a{};
        a.type = SceneActionType::SetMouseWarpping;
        a.setMouseWarpping.enabled = false;
        f.outSceneActions.emplace_back(a);
        isKeyMouseUsed = false;
    }
}
}  // namespace OneGame::Engine::ECS
