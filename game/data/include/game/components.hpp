#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

#include "game/game_world.hpp"
#include "oge/aabb.hpp"
#include "oge/input/raw_input_stream.hpp"
#include "oge/math.hpp"
#include "oge/point3.hpp"
#include "oge/runtime/entt.hpp"
#include "oge/runtime/net_serializer.hpp"

namespace game
{
namespace math = ::oge::math;
using oge::AABB;
using oge::Point3;

struct DirtyTag
{
    oge::BitSet256 dirtyComponents;
};

enum class UpdateType
{
    FixedStep,
    Realtime,
};

template <UpdateType>
struct UpdateTag
{
};

// =========================================================================
// Render strategy tags — control how the interpolation/prediction layer
// treats each entity.  These are pure ECS marker components; no
// DECL_TYPE_NAME needed.
// =========================================================================

enum class RenderStrategy
{
    None,                 // No special rendering — use physics state as-is
    Interpolation,        // Lerp between last two fixed-step states
    Extrapolation,        // Project forward from velocity (reserved)
    FixedStepSmoothing,   // Smooth remote entity updates (reserved)
    LocalPrediction,      // Client-side prediction with rollback
};

template <RenderStrategy>
struct RenderStrategyTag
{
};

// Render-only smoothed transform — renderers prefer this over raw
// ComponentPhysicBody when present.
struct ComponentInterpolatedTransform
{
    math::vec3 pos{};
};

struct ComponentCamera
{
    float yaw = 0.f;
    float pitch = 0.f;
    math::vec3 position = {};
    math::vec3 forward = {};

    math::vec3 up() const
    {
        return glm::cross(right(), forward);
    }

    math::vec3 right() const
    {
        static glm::vec3 worldUp(0, 1, 0);
        return glm::normalize(glm::cross(forward, worldUp));
    }

    ComponentCamera(math::vec3 position = {})
    {
        SetYawPitch(0.f, 0.f);
    }

    ComponentCamera(math::vec3 position, math::vec3 forward)
    {
        yaw = std::atan2(forward.x, forward.z);
        pitch = std::asin(forward.y);
    }

    math::mat4 view() const;
    void ApplyDelta(float dsx, float dsy);
    void SetYawPitch(float yaw, float pitch);
};

struct ComponentPerspectiveCamera
{
    float fov = math::radians(45.0f);
    float aspect = 1.f;
};

math::vec3 ScreenToRay(ComponentCamera camera,
                       ComponentPerspectiveCamera pcamera, math::vec2 pos);
math::vec2 ScreenToView(ComponentPerspectiveCamera pcamera, math::vec2 pos);
math::vec3 ViewToRay(ComponentCamera camera, math::vec2 pos);

struct InputSourceWidget
{
    entt::entity moveWidget;
    entt::entity viewWidget;
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

struct PlayerInfo
{
    std::array<uint8_t, 16> uuid;
    math::vec3 latestPosition;
};

struct ComponentPlayer
{
    std::array<uint8_t, 16> id;
    float lastActionTime = 0.f;
    uint64_t inputCursor{};
    uint64_t actionCursor{};

    static entt::entity CreatePlayer(GameWorld& world, PlayerInfo info,
                                     entt::entity hint = entt::null);
    static void DestroyPlayer(GameWorld& world, PlayerInfo info);
};

struct ComponentTargetBlock
{
    Point3 hitPos = {};   // integer block coords the player is looking at
    bool valid = false;
};

struct DebugText
{
    std::pmr::string text;
    float remainingTime = 0.f;
};

}  // namespace game

template <game::UpdateType utype>
struct oge::runtime::TypeName<game::UpdateTag<utype>>
{
    static constexpr std::string Get()
    {
        return utype == game::UpdateType::FixedStep
                   ? "core::SubsystemPlayer<FixedStep>"
                   : "core::SubsystemPlayer<Realtime>";
    }
};

DECL_TYPE_NAME(game::DirtyTag, "core::DirtyTag")
DECL_TYPE_NAME(game::ComponentPlayer, "core::ComponentPlayer")
DECL_TYPE_NAME(game::ComponentTargetBlock, "core::ComponentTargetBlock")
DECL_TYPE_NAME(game::ComponentAABBCollider, "core::ComponentAABBCollider")
DECL_TYPE_NAME(game::ComponentCamera, "core::ComponentCamera")
DECL_TYPE_NAME(game::ComponentPerspectiveCamera, "core::ComponentPerspectiveCamera")
DECL_TYPE_NAME(game::ComponentCreature, "core::ComponentCreature")
DECL_TYPE_NAME(game::ComponentPhysicBody, "core::ComponentPhysicBody")
DECL_TYPE_NAME(game::UpdateTag<game::UpdateType::FixedStep>, "core::UpdateTag<FixedStep>")
DECL_TYPE_NAME(game::UpdateTag<game::UpdateType::Realtime>, "core::UpdateTag<Realtime>")
DECL_TYPE_NAME(game::RenderStrategyTag<game::RenderStrategy::Interpolation>, "core::RenderStrategyTag<Interpolation>")
DECL_TYPE_NAME(game::RenderStrategyTag<game::RenderStrategy::LocalPrediction>, "core::RenderStrategyTag<LocalPrediction>")
DECL_TYPE_NAME(game::ComponentInterpolatedTransform, "core::ComponentInterpolatedTransform")
