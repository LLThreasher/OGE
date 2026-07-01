#pragma once
#include "Engine/GameAppState.hpp"
#include "Engine/Graphics/IGraphicsBackend.hpp"
#include "Engine/Graphics/PresentationObjects.hpp"
#include "Engine/Input/InputSystem.hpp"
#include "Engine/Math.hpp"
#include "Engine/Point2.hpp"
#include "Engine/Terrain/TerrainView.hpp"

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


namespace OneGame::Engine::Terrain
{
class BlockRegistry;
}

namespace OneGame::Engine::ECS
{

struct TerrainContext
{
    entt::registry& world;
    Terrain::BlockRegistry& blocks;
};

struct GameWorldContext : TerrainContext
{
    Terrain::TerrainView& terrain;
};

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

struct ComponentCamera
{
    float yaw;
    float pitch;
    math::vec3 position;
    glm::vec3 forward;
    entt::entity targetPanel;

    math::mat4 view() const;
    void ApplyDelta(float dsx, float dsy, float dwx, float dwz);
};

struct ComponentPerspectiveCamera
{
    float fov = math::radians(45.0f);
    float aspect = 1.f;
};

math::vec3 ScreenToRay(ComponentCamera camera, ComponentPerspectiveCamera pcamera, math::vec2 pos);

math::vec2 RayToPitchYaw(math::vec3 ray);

DECLARE_SUBSYSTEM(Camera, void onViewPanelUpdate(entt::registry& world, entt::entity entity););

DECLARE_SUBSYSTEM(DebugInfo, float currentFPS = 0.f; float currentFrameTime = 0.f; float accumTime = 0.f;
                  uint64_t frameCount = 0; FramePerfStatus perfStatus;);

void AddDebugInfo(entt::registry& presentationWorld, std::string_view msg);

struct UIRect;
struct ScreenRect;
struct PlayerInputData
{
    math::vec2 moveDelta;
    math::vec2 panDelta;
    bool digging;
    math::vec2 diggingPos;
    bool placing;
    math::vec2 placingPos;
};
DECLARE_SUBSYSTEM(PlayerInput,
    void onCreateInputSourceKeyMouse(entt::registry& gameWorld, entt::entity entity);
    void onEraseInputSourceKeyMouse(entt::registry& gameWorld, entt::entity entity);
    void onEraseInputSourceWidget(entt::registry& gameWorld, entt::entity entity);
    bool isKeyMouseUsed = false; bool previousIsKeyMouseUsed = false;);

struct UIDrag
{
    int inputIndex = -1;
    MouseButton dragStartButton = MouseButton::Left;
    entt::entity onTopOf = entt::null;
    math::vec2 dragStartPos;
    math::vec2 dragLastPos;
    math::vec2 dragDelta = {};

    void UpdateDrag(math::vec2 pos, entt::entity onTopOf)
    {
        dragDelta = pos - dragLastPos;
        dragLastPos = pos;
        onTopOf = onTopOf;
    }
};

struct UIDragRelease
{
    UIDrag drag;
    entt::entity dragStart;
};

struct UIRaycastTarget
{
};

struct UIFocus
{
};

struct UIRaycastHit
{
};

struct UIRect
{
    int zLevel;
    math::vec2 pos;
    math::vec2 extent;
};

struct UIRoot
{
};

struct UIParent
{
    entt::entity parent;
};

struct ScreenRect
{
    Point2 pos;
    UPoint2 extent;
};

struct InputSourceWidget
{
    entt::entity moveWidget;
    entt::entity viewWidget;
};

struct InputSourceKeyMouse
{
};

struct ViewPanel
{
    Graphics::GameViewType activeSlot = Graphics::GameViewType::Slot0;
    entt::entity activeCamera = entt::null;
};

// Handle drags and UI rendering
DECLARE_SUBSYSTEM(UI, std::array<entt::entity, PointerIdx::COUNT> activeDrags;);

struct ComponentPhysicBody
{
    math::vec3 pos;
    math::vec3 velocity;
};

struct ComponentPlayer
{
    std::optional<Terrain::TerrainRaycastResult> lookingAt;

    static entt::entity CreatePlayer(entt::registry& world)
    {
        auto res = world.create();
        world.emplace<ComponentPhysicBody>(res);
        world.emplace<ComponentCamera>(res);
        world.emplace<ComponentPerspectiveCamera>(res);
        world.emplace<ComponentPlayer>(res);
        world.emplace<PlayerInputData>(res);
        return res;
    }
};
DECLARE_SUBSYSTEM(Player);
}  // namespace OneGame::Engine::ECS

namespace OneGame::Engine::UI
{
math::vec2 ScreenSpaceToRelSpace(const ECS::ScreenRect rect, math::vec2 screenPos);
math::vec2 ScreenSpaceToRelSpace(entt::registry& world, entt::entity rectEntity, math::vec2 screenPos);
math::vec2 ScreenSpaceToRelSpace(entt::registry& world, math::vec2 screenPos);
Point2 RelSpaceToScreenSpace(entt::registry& world, math::vec2 relPos);
ECS::ScreenRect UIRectToScreenRect(entt::registry& world, entt::entity rect);
entt::entity CastRayScreenSpace(entt::registry& gameWorld, math::vec2 pos);
entt::entity CastRayRelSpace(entt::registry& gameWorld, math::vec2 pos);
entt::entity CreateGameView(entt::registry& game, const ECS::UIRect rect);
}