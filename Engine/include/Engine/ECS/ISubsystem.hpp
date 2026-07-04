#pragma once
#include "Engine/GameAppState.hpp"
#include "Engine/Graphics/IGraphicsBackend.hpp"
#include "Engine/Graphics/PresentationObjects.hpp"
#include "Engine/Input/InputSystem.hpp"
#include "Engine/Math.hpp"
#include "Engine/Point2.hpp"
#include "Engine/Rect.hpp"
#include "Engine/Terrain/TerrainView.hpp"
#include "Components.hpp"

#define DECLARE_SUBSYSTEM(Name, ...)                                                                       \
    class Subsystem##Name : public SubsystemBase                                                           \
    {                                                                                                      \
       public:                                                                                             \
        void Initialize(GameWorldContext& game, AppContext ctx) override;                                  \
        void Update(GameWorldContext& game, AppContext ctx, const FrameInputData& fd) override;            \
        void Present(const GameWorldContext& game, PresentationContext ctx, FrameOutputData& fd) override; \
                                                                                                           \
       private:                                                                                            \
        __VA_ARGS__                                                                                        \
    };

namespace OneGame::Engine::ECS
{
using UIRect = FRect;
using ScreenRect = IRect;
}

namespace OneGame::Engine::UI
{
math::vec2 ScreenSpaceToRelSpace(const ECS::ScreenRect rect, math::vec2 screenPos);
math::vec2 ScreenSpaceToRelSpace(const entt::registry& world, entt::entity rectEntity, math::vec2 screenPos);
math::vec2 ScreenSpaceToRelSpace(const entt::registry& world, math::vec2 screenPos);
Point2 RelSpaceToScreenSpace(const entt::registry& world, math::vec2 relPos);
ECS::ScreenRect UIRectToScreenRect(const entt::registry& world, entt::entity rect);
entt::entity CastRayScreenSpace(const entt::registry& gameWorld, math::vec2 pos);
entt::entity CastRayRelSpace(const entt::registry& gameWorld, math::vec2 pos);
entt::entity CreateGameView(entt::registry& game, ECS::UIRect rect);
}

namespace OneGame::Engine::ECS
{

using TerrainContext = entt::registry;
using GameWorldContext = entt::registry;

template <typename TData>
class ISubsystem
{
   public:
    virtual ~ISubsystem() = default;
    virtual void Initialize(TData& game, AppContext ctx) = 0;
    virtual void Update(TData& game, AppContext ctx, const FrameInputData& fd) = 0;
    virtual void Present(const TData& game, PresentationContext ctx, FrameOutputData& fd) = 0;
};

class SubsystemBase : public ISubsystem<GameWorldContext>
{
};

math::vec3 ScreenToRay(ComponentCamera camera, ComponentPerspectiveCamera pcamera, math::vec2 pos);

math::vec2 RayToPitchYaw(math::vec3 ray);

DECLARE_SUBSYSTEM(Camera, void onViewPanelUpdate(entt::registry& world, entt::entity entity););

DECLARE_SUBSYSTEM(DebugInfo, float currentFPS = 0.f; float currentFrameTime = 0.f; float accumTime = 0.f;
                  uint64_t frameCount = 0; FramePerfStatus totalPerfStatus; FramePerfStatus perfStatus;);

void AddDebugInfo(entt::registry& presentationWorld, std::string_view msg);

DECLARE_SUBSYSTEM(PlayerInput,
    void onCreateInputSourceKeyMouse(entt::registry& gameWorld, entt::entity entity);
    void onEraseInputSourceKeyMouse(entt::registry& gameWorld, entt::entity entity);
    void onEraseInputSourceWidget(entt::registry& gameWorld, entt::entity entity);
    bool isKeyMouseUsed = false; bool previousIsKeyMouseUsed = false;);

// Handle drags and UI rendering
DECLARE_SUBSYSTEM(UI, std::array<entt::entity, PointerIdx::COUNT> activeDrags;);

DECLARE_SUBSYSTEM(Player);
}  // namespace OneGame::Engine::ECS
