#pragma once

#include "Components.hpp"
#include "Engine/GameAppState.hpp"
#include "Engine/Input/InputSystem.hpp"
#include "Engine/Math.hpp"
#include "Engine/Point2.hpp"
#include "Engine/Rect.hpp"
#include "Engine/entt.hpp"

#define DECL_SUBSYSTEM(MName, ...)                                                              \
    class Subsystem##MName : public SubsystemBase                                               \
    {                                                                                           \
       public:                                                                                  \
        static constexpr std::string_view Name = "Subsystem" #MName;                                 \
        void Initialize(GameWorldContext& game, AppContext ctx) override;                       \
        void Update(GameWorldContext& game, AppContext ctx, const FrameInputData& fd) override; \
                                                                                                \
       private:                                                                                 \
        __VA_ARGS__                                                                             \
    };

#define DECL_SUBSYSTEM_INIT(Name) void Subsystem##Name::Initialize(GameWorldContext& game, AppContext engine)
#define DECL_SUBSYSTEM_UPDATE(Name) \
    void Subsystem##Name::Update(GameWorldContext& game, AppContext engine, const FrameInputData& frame)

namespace OneGame::Engine::ECS
{
struct UIRect;
struct ScreenRect;
}  // namespace OneGame::Engine::ECS

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
};

class SubsystemBase : public ISubsystem<GameWorldContext>
{
};

math::vec3 ScreenToRay(ComponentCamera camera, ComponentPerspectiveCamera pcamera, math::vec2 pos);

math::vec2 RayToPitchYaw(math::vec3 ray);

DECL_SUBSYSTEM(PlayerInput, void onEraseInputSourceKeyMouse(entt::registry& gameWorld, entt::entity entity);
               void onEraseInputSourceWidget(entt::registry& gameWorld, entt::entity entity););

// Handle drags and UI rendering
DECL_SUBSYSTEM(UI, std::array<entt::entity, PointerIdx::COUNT> activeDrags;);

DECL_SUBSYSTEM(Player);
DECL_SUBSYSTEM(Physics);
DECL_SUBSYSTEM(Creature);

}  // namespace OneGame::Engine::ECS
