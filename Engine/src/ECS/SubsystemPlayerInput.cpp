#include "Engine/ECS/ISubsystem.hpp"
#include "Engine/Graphics/PresentationObjects.hpp"
#include "Engine/Math.hpp"

namespace OneGame::Engine::ECS
{
void SubsystemPlayerInput::Initialize(GameWorldContext& game, AppContext ctx)
{
    game.world.on_construct<InputSourceKeyMouse>().connect<&SubsystemPlayerInput::onCreateInputSourceKeyMouse>(this);
    game.world.on_destroy<InputSourceKeyMouse>().connect<&SubsystemPlayerInput::onEraseInputSourceKeyMouse>(this);
    game.world.on_destroy<InputSourceWidget>().connect<&SubsystemPlayerInput::onEraseInputSourceWidget>(this);
}

void SubsystemPlayerInput::onCreateInputSourceKeyMouse(entt::registry& gameWorld, entt::entity entity)
{
    isKeyMouseUsed = true;
}

void SubsystemPlayerInput::onEraseInputSourceKeyMouse(entt::registry& gameWorld, entt::entity entity)
{
    isKeyMouseUsed = gameWorld.view<InputSourceKeyMouse>().size() > 1;
    if (!gameWorld.any_of<InputSourceWidget>(entity))
    {
        gameWorld.replace<PlayerInputData>(entity, PlayerInputData{});
    }
}

void SubsystemPlayerInput::onEraseInputSourceWidget(entt::registry& gameWorld, entt::entity entity)
{
    if (!gameWorld.any_of<InputSourceKeyMouse>(entity))
    {
        gameWorld.replace<PlayerInputData>(entity, PlayerInputData{});
    }
}

void SubsystemPlayerInput::Update(GameWorldContext& game, AppContext ctx, const FrameInputData& f)
{
    for (auto [entity, data] :
         game.world.view<PlayerInputData>().each())
    {
        // handle touch
        if (auto widgetInput = game.world.try_get<InputSourceWidget>(entity))
        {
            // handle move
            {
                auto drag = game.world.try_get<UIDrag>(widgetInput->moveWidget);
                if (drag != nullptr)
                {
                    auto touchIdx = PointerIdx::TouchIdxFromPtrIdx(drag->inputIndex);
                    math::vec2 pos = {f.input.GetTouchX(touchIdx), f.input.GetTouchY(touchIdx)};
                    data.moveDelta = UI::ScreenSpaceToRelSpace(game.world, widgetInput->moveWidget, pos - drag->dragStartPos);
                }
            }
            // handle pan
            {
                auto drag = game.world.try_get<UIDrag>(widgetInput->viewWidget);
                if (drag != nullptr)
                {
                    auto touchIdx = PointerIdx::TouchIdxFromPtrIdx(drag->inputIndex);
                    math::vec2 posDelta = {f.input.GetTouchDX(touchIdx), f.input.GetTouchDY(touchIdx)};
                    data.panDelta = UI::ScreenSpaceToRelSpace(game.world, widgetInput->viewWidget, posDelta);
                }
            }
        }
        // handle keyboard 
        if (game.world.any_of<InputSourceKeyMouse>(entity))
        {
            data.panDelta = 0.001f * math::vec2{f.input.GetMouseDX(), -f.input.GetMouseDY()};
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
    if (isKeyMouseUsed && !previousIsKeyMouseUsed)
    {
        SceneAction a{};
        a.type = SceneActionType::SetMouseWarpping;
        a.setMouseWarpping.enabled = true;
        f.outSceneActions.emplace_back(a);
        previousIsKeyMouseUsed = true;
    }
    if (!isKeyMouseUsed && previousIsKeyMouseUsed)
    {
        SceneAction a{};
        a.type = SceneActionType::SetMouseWarpping;
        a.setMouseWarpping.enabled = false;
        f.outSceneActions.emplace_back(a);
        previousIsKeyMouseUsed = false;
    }
}
}  // namespace OneGame::Engine::ECS
