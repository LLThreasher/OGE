#include "Engine/ECS/ISubsystem.hpp"
#include "Engine/Graphics/PresentationObjects.hpp"
#include "Engine/Math.hpp"
#include "Engine/Formaters.hpp"

namespace OneGame::Engine::ECS
{
void SubsystemPlayerInput::Initialize(GameWorldContext& game, AppContext ctx)
{
    game.on_construct<InputSourceKeyMouse>().connect<&SubsystemPlayerInput::onCreateInputSourceKeyMouse>(this);
    game.on_destroy<InputSourceKeyMouse>().connect<&SubsystemPlayerInput::onEraseInputSourceKeyMouse>(this);
    game.on_destroy<InputSourceWidget>().connect<&SubsystemPlayerInput::onEraseInputSourceWidget>(this);
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
         game.view<PlayerInputData>().each())
    {
        // handle touch
        if (auto widgetInput = game.try_get<InputSourceWidget>(entity))
        {
            // handle move
            {
                auto drag = game.try_get<UIDrag>(widgetInput->moveWidget);
                if (drag != nullptr)
                {
                    data.moveDelta = drag->dragLastPos - drag->dragStartPos;
                    data.moveDelta = -data.moveDelta;
                }
                else
                {
                    data.moveDelta = math::vec2{0, 0};
                }
            }
            // handle pan
            {
                auto drag = game.try_get<UIDrag>(widgetInput->viewWidget);
                if (drag != nullptr)
                {
                    if (drag->deltaTime > 0.5f && (data.get<PlayerAction::Digging>() || drag->IsHold(game)))
                    {
                        data.set<PlayerAction::Digging>();
                        data.actionPos= drag->dragLastPos;
                        data.panDelta = math::vec2{0, 0};
                    }
                    else
                    {
                        data.unset<PlayerAction::Digging>();
                        auto pcam = game.get<ComponentPerspectiveCamera>(entity);
                        auto vfov = pcam.fov;
                        auto hfov = -2.f * math::atan(math::tan(pcam.fov / 2.f) * pcam.aspect);
                        data.panDelta = drag->dragDelta * math::vec2{hfov, vfov};   
                    }
                }
                else
                {
                    data.unset<PlayerAction::Digging>();
                    data.panDelta = math::vec2{0, 0};
                }
                auto dragRl = game.try_get<UIDragRelease>(widgetInput->viewWidget);
                if (dragRl != nullptr && dragRl->dragStart == widgetInput->viewWidget && dragRl->drag.IsClick(game))
                {
                    data.set<PlayerAction::Placing>();
                    data.actionPos = dragRl->drag.dragLastPos;
                }
                else
                {
                    data.unset<PlayerAction::Placing>();
                }
            }
        }
        // handle keyboard & mouse
        if (game.any_of<InputSourceKeyMouse>(entity))
        {
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

            auto pcam = game.get<ComponentPerspectiveCamera>(entity);
            auto vfov = -pcam.fov;
            auto hfov = 2.f * math::atan(math::tan(pcam.fov / 2.f) * pcam.aspect);
            data.panDelta = UI::ScreenSpaceToRelSpace(game, math::vec2{f.input.GetMouseDX(), f.input.GetMouseDY()}) * math::vec2{hfov, vfov};
            
            if (f.input.IsMouseDown(MouseButton::Left))
            {
                data.set<PlayerAction::Digging>();
                data.actionPos = {0.5f, 0.5f};
            }
            else
            {
                data.unset<PlayerAction::Digging>();
            }
            if (f.input.IsMouseDown(MouseButton::Right))
            {
                data.set<PlayerAction::Placing>();
                data.actionPos = {0.5f, 0.5f};
            }
            else
            {
                data.unset<PlayerAction::Placing>();
            }
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
