#pragma once
#include "Engine/GameAppState.hpp"
#include "Engine/Graphics/IGraphicsBackend.hpp"
#include "Engine/Graphics/PresentationObjects.hpp"
#include "Engine/Input/InputSystem.hpp"
#include "Engine/Math.hpp"
#include "Engine/Point2.hpp"
#include "Engine/Rect.hpp"
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
                  uint64_t frameCount = 0; FramePerfStatus totalPerfStatus; FramePerfStatus perfStatus;);

void AddDebugInfo(entt::registry& presentationWorld, std::string_view msg);

enum class PlayerAction : uint32_t
{
    Digging = 0,
    Placing,
};
struct PlayerInputData
{
    math::vec2 moveDelta;
    math::vec2 panDelta;
    math::vec2 actionPos;
    uint8_t actionMask;

    template<PlayerAction action>
    inline bool get() const
    {
        return actionMask & (1 << static_cast<uint32_t>(action));
    }

    template<PlayerAction action>
    inline void set()
    {
        actionMask |= (1 << static_cast<uint32_t>(action));
    }

    template<PlayerAction action>
    inline void unset()
    {
        actionMask &= ~(1 << static_cast<uint32_t>(action));
    }
};

DECLARE_SUBSYSTEM(PlayerInput,
    void onCreateInputSourceKeyMouse(entt::registry& gameWorld, entt::entity entity);
    void onEraseInputSourceKeyMouse(entt::registry& gameWorld, entt::entity entity);
    void onEraseInputSourceWidget(entt::registry& gameWorld, entt::entity entity);
    bool isKeyMouseUsed = false; bool previousIsKeyMouseUsed = false;);

struct UISprite
{
    GPUTextureHandle texture;
};

struct UIDrag
{
    int inputIndex = -1;
    MouseButton dragStartButton = MouseButton::Left;
    entt::entity onTopOf = entt::null;
    math::vec2 dragStartPos;
    math::vec2 dragLastPos;
    float deltaTime = 0.f;
    math::vec2 dragDelta = {};

    void UpdateDrag(math::vec2 pos, entt::entity onTopOf, float dt)
    {
        dragDelta = pos - dragLastPos;
        dragLastPos = pos;
        onTopOf = onTopOf;
        deltaTime += dt;
    }

    bool IsHold(const entt::registry& world, int pixelRadiusSqr = 200) const
    {
        auto diff = UI::RelSpaceToScreenSpace(world, dragStartPos - dragLastPos);
        return diff.x * diff.x + diff.y * diff.y < pixelRadiusSqr;
    }

    bool IsClick(const entt::registry& world, float duration = 0.25f, int pixelRadiusSqr = 200) const
    {
        if (deltaTime > duration) return false;
        return IsHold(world, pixelRadiusSqr);
    }
};

struct UIDragRelease
{
    UIDrag drag;
    entt::entity dragStart;
};

struct UIZLevel
{
    int zLevel = 0;
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

struct UIRoot
{
};

struct UIParent
{
    entt::entity parent;
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
