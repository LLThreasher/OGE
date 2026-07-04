#pragma once

#include <optional>

#include "Engine/entt.hpp"
#include "Engine/Math.hpp"
#include "Engine/Rect.hpp"
#include "Engine/Input/InputSystem.hpp"
#include "Engine/ObjectType.hpp"
#include "Engine/Graphics/PresentationObjects.hpp"
#include "Engine/Terrain/TerrainView.hpp"

namespace OneGame::Engine::ECS
{

using TerrainRaycastResult = Terrain::TerrainRaycastResult;
using UIRect = FRect;
using ScreenRect = IRect;

struct ComponentCamera
{
    float yaw;
    float pitch;
    math::vec3 position;
    math::vec3 forward;
    entt::entity targetPanel;

    math::vec3 up() const
    {
        return glm::cross(right(), forward);
    }

    math::vec3 right() const
    {
        static glm::vec3 worldUp(0,1,0);
        return glm::normalize(glm::cross(forward, worldUp));
    }

    math::mat4 view() const;
    void ApplyDelta(float dsx, float dsy, float dwx, float dwz);
};

struct ComponentPerspectiveCamera
{
    float fov = math::radians(45.0f);
    float aspect = 1.f;
};

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

    bool IsHold(const entt::registry& world, int pixelRadiusSqr = 200) const;

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

struct SwapchainExtent : UPoint2
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

struct ComponentPhysicBody
{
    math::vec3 pos;
    math::vec3 velocity;
};

struct ComponentPlayer
{
    std::optional<TerrainRaycastResult> lookingAt;

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

namespace OneGame::Engine::UI
{
    math::vec2 RayToPitchYaw(math::vec3 ray);
    math::vec3 ScreenToRay(ComponentCamera camera, ComponentPerspectiveCamera pcamera, math::vec2 pos);
    math::vec2 ScreenSpaceToRelSpace(const ScreenRect rect, math::vec2 screenPos);
    math::vec2 ScreenSpaceToRelSpace(const entt::registry& world, entt::entity rectEntity,
                                     math::vec2 screenPos);
    math::vec2 ScreenSpaceToRelSpace(const entt::registry& world, math::vec2 screenPos);
    Point2 RelSpaceToScreenSpace(const entt::registry& world, math::vec2 relPos);
    ScreenRect UIRectToScreenRect(const entt::registry& world, entt::entity rect);
    entt::entity CastRayScreenSpace(const entt::registry& gameWorld, math::vec2 pos);
    entt::entity CastRayRelSpace(const entt::registry& gameWorld, math::vec2 pos);
    entt::entity CreateGameView(entt::registry & game, UIRect rect);
}
}  // namespace OneGame::Engine::ECS
