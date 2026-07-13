#pragma once

#include <optional>

#include "Engine/AABB.hpp"
#include "Engine/Input/InputSystem.hpp"
#include "Engine/Math.hpp"
#include "Engine/ObjectType.hpp"
#include "Engine/Rect.hpp"
#include "Engine/Terrain/TerrainView.hpp"
#include "Engine/entt.hpp"

namespace OneGame::Engine::ECS
{

using TerrainRaycastResult = Terrain::TerrainRaycastResult;
struct UIRect;

struct ComponentCamera
{
    float yaw;
    float pitch;
    math::vec3 position;
    math::vec3 forward;
    entt::entity targetPanel;

    math::vec3 up() const { return glm::cross(right(), forward); }

    math::vec3 right() const
    {
        static glm::vec3 worldUp(0, 1, 0);
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
    Jump,
};

struct PlayerInputData
{
    math::vec2 moveDelta;
    math::vec2 panDelta;
    math::vec2 actionPos;
    uint8_t actionMask;

    template <PlayerAction action>
    inline bool get() const
    {
        return actionMask & (1 << static_cast<uint32_t>(action));
    }

    template <PlayerAction action>
    inline void set()
    {
        actionMask |= (1 << static_cast<uint32_t>(action));
    }

    inline bool empty() const { return actionMask == 0; }

    template <PlayerAction action>
    inline void unset()
    {
        actionMask &= ~(1 << static_cast<uint32_t>(action));
    }
};

struct InputSourceWidget
{
    entt::entity moveWidget;
    entt::entity viewWidget;
};

struct InputSourceKeyMouse
{
};

struct ComponentPhysicBody
{
    math::vec3 pos = {};
    math::vec3 velocity = {};
    math::vec3 acceleration = {};
    float mass = 1.0f;
    float stepAssist = 0.01f;
    uint32_t onTopOfBlkValue = 0;
    bool isGrounded = false;
    bool enableGravity = true;
};

struct ComponentCreature
{
    float maxSpeed = 1.f;
    float initJumpSpeed = math::sqrt(2.f * 1.55f * 9.8f);
    math::vec3 moveOrder = {};
    bool jumpOrder = false;

    void SetMaxJumpHeight(float height)
    {
        initJumpSpeed = math::sqrt(2.f * height * 9.8f);
    }
};

struct ComponentCreatureInfo
{
    float moveForce;
    float jumpForce;
    float stepAssist;
};

struct ComponentAABBCollider
{
    AABB aabb;
};

struct ComponentPlayer
{
    float lastActionTime = 0.f;

    static entt::entity CreatePlayer(entt::registry& world, math::vec3 pos);
};

struct DebugText
{
    
};

namespace OneGame::Engine::UI
{
math::vec2 RayToPitchYaw(math::vec3 ray);
math::vec3 ScreenToRay(ComponentCamera camera, ComponentPerspectiveCamera pcamera, math::vec2 pos);
entt::entity CastRayRelSpace(const entt::registry& gameWorld, math::vec2 pos);
}  // namespace OneGame::Engine::UI
}  // namespace OneGame::Engine::ECS
