#pragma once

#include "oge/aabb.hpp"
#include "oge/math.hpp"
#include "oge/runtime/entt.hpp"

namespace game::math
{
    using namespace oge::math;
}

namespace game
{
using oge::AABB;
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
    void ApplyDelta(float dsx, float dsy);
};

struct ComponentPerspectiveCamera
{
    float fov = math::radians(45.0f);
    float aspect = 1.f;
};

math::vec3 ScreenToRay(ComponentCamera camera, ComponentPerspectiveCamera pcamera, math::vec2 pos);

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
    math::vec2 lookOrder = {};
    bool jumpOrder = false;

    void SetMaxJumpHeight(float height) { initJumpSpeed = math::sqrt(2.f * height * 9.8f); }
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
    std::string text;
};

}  // namespace game
