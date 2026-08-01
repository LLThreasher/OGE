#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

#include "game/game_world.hpp"
#include "oge/aabb.hpp"
#include "oge/input/raw_input_stream.hpp"
#include "oge/math.hpp"
#include "oge/runtime/entt.hpp"
#include "oge/runtime/net_serializer.hpp"

namespace game
{
namespace math = ::oge::math;
using oge::AABB;

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

struct ReplicatedTag
{
};

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

math::vec3 ScreenToRay(ComponentCamera camera,
                       ComponentPerspectiveCamera pcamera, math::vec2 pos);

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

    void Serialize(net::Buffer& buffer)
    {
        buffer.Write(maxSpeed);
        buffer.Write(initJumpSpeed);
    }

    void Deserialize(net::Buffer& buffer)
    {
        maxSpeed = buffer.Read<float>();
        initJumpSpeed = buffer.Read<float>();
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

    void Serialize(net::Buffer& buffer)
    {
        buffer.Write(aabb);
    }

    void Deserialize(net::Buffer& buffer)
    {
        aabb =  buffer.Read<AABB>();
    }
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

    static entt::entity CreatePlayer(entt::registry& world, PlayerInfo info, entt::entity hint = entt::null);
    static void DestroyPlayer(entt::registry& world, PlayerInfo info);

    void Serialize(net::Buffer& buffer)
    {
        buffer.Write<std::array<uint8_t, 16>>(id);
    }

    void Deserialize(net::Buffer& buffer)
    {
        id = buffer.Read<std::array<uint8_t, 16>>();
    }

    size_t Size()
    {
        return sizeof(uint8_t) * 16;
    }
};

struct DebugText
{
    std::pmr::string text;
    float remainingTime = 0.f;
};

}  // namespace game

namespace oge::runtime
{
template <>
struct TypeName<game::ReplicatedTag>
{
    static constexpr std::string Get()
    {
        return "core::ReplicatedTag";
    }
};

template <>
struct TypeName<game::ComponentPlayer>
{
    static constexpr std::string Get()
    {
        return "core::ComponentPlayer";
    }
};
}  // namespace oge::runtime
