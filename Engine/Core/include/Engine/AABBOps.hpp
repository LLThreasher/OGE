#pragma once

#include "Engine/AABB.hpp"

namespace OneGame::Engine
{

constexpr uint32_t COLLISION_TYPE_POS_Y = 0;
constexpr uint32_t COLLISION_TYPE_NEG_Y = 1;
constexpr uint32_t COLLISION_TYPE_POS_X = 2;
constexpr uint32_t COLLISION_TYPE_NEG_X = 3;
constexpr uint32_t COLLISION_TYPE_POS_Z = 4;
constexpr uint32_t COLLISION_TYPE_NEG_Z = 5;
constexpr uint32_t COLLISION_TYPE_STEP_Y = 6;

const math::vec3 COLLISION_NORMALS[] = {
    math::vec3{0.f, 1.f, 0.f}, math::vec3{0.f, -1.f, 0.f}, math::vec3{1.f, 0.f, 0.f}, math::vec3{-1.f, 0.f, 0.f},
    math::vec3{0.f, 0.f, 1.f}, math::vec3{0.f, 0.f, -1.f}, math::vec3{0.f, 1.f, 0.f}, math::vec3{0.f, -1.f, 0.f},
};

constexpr float COLLISION_EPSILON = 0.001f;

struct CollisionResult
{
    uint32_t type;
    float penetration = 0.f;
};

constexpr uint32_t RCR_HIT_POS_X = 1 << 0;
constexpr uint32_t RCR_HIT_POS_Y = 1 << 1;
constexpr uint32_t RCR_HIT_POS_Z = 1 << 2;
constexpr uint32_t RCR_HIT_NEG_X = 1 << 3;
constexpr uint32_t RCR_HIT_NEG_Y = 1 << 4;
constexpr uint32_t RCR_HIT_NEG_Z = 1 << 5;
constexpr uint32_t RCR_HIT_STEP_Y = 1 << 6;

struct ResolveCollisionResult
{
    math::vec3 offset = {0.f, 0.f, 0.f};
    float stepOffset = 0.f;
    uint32_t mask = 0;
};

inline void ResolveCollisionsSinglePass(const std::vector<CollisionResult>& collisions, ResolveCollisionResult& res)
{
    if (collisions.empty())
    {
        res = {};
        return;
    }

    // Separate tracking for positive (+) and negative (-) pushes on each axis
    float maxPosX = 0.0f;
    float maxNegX = 0.0f;
    float maxPosY = 0.0f;
    float maxNegY = 0.0f;
    float maxPosZ = 0.0f;
    float maxNegZ = 0.0f;
    float maxSAPosY = 0.0f;

    for (const auto& col : collisions)
    {
        auto& normal = COLLISION_NORMALS[col.type];
        float pushX = normal.x * col.penetration;
        float pushY = normal.y * col.penetration;
        float pushZ = normal.z * col.penetration;

        // X-Axis sorting by sign
        if (pushX > 0.0f)
        {
            maxPosX = std::max(maxPosX, pushX + COLLISION_EPSILON);
            res.mask |= RCR_HIT_POS_X;
        }
        else if (pushX < 0.0f)
        {
            maxNegX = std::min(maxNegX, pushX - COLLISION_EPSILON);  // Keeps the most negative value
            res.mask |= RCR_HIT_NEG_X;
        }

        // Y-Axis sorting by sign
        if (col.type == COLLISION_TYPE_STEP_Y)
        {
            maxSAPosY = std::max(maxSAPosY, pushY + COLLISION_EPSILON);
            res.mask |= RCR_HIT_STEP_Y;
        }
        else if (pushY > 0.0f)
        {
            maxPosY = std::max(maxPosY, pushY + COLLISION_EPSILON);
            res.mask |= RCR_HIT_POS_Y;
        }
        else if (pushY < 0.0f)
        {
            maxNegY = std::min(maxNegY, pushY - COLLISION_EPSILON);
            res.mask |= RCR_HIT_NEG_Y;
        }

        // Z-Axis sorting by sign
        if (pushZ > 0.0f)
        {
            maxPosZ = std::max(maxPosZ, pushZ + COLLISION_EPSILON);
            res.mask |= RCR_HIT_POS_Z;
        }
        else if (pushZ < 0.0f)
        {
            maxNegZ = std::min(maxNegZ, pushZ - COLLISION_EPSILON);
            res.mask |= RCR_HIT_NEG_Z;
        }
    }

    // Combine opposing forces by adding them together.
    // If stuck between two walls: (+2.0) + (-2.0) = 0.0 total movement. The player stays in the center.
    res.offset.x = maxPosX + maxNegX;
    res.offset.z = maxPosZ + maxNegZ;
    res.offset.y = maxPosY + maxNegY;
    res.stepOffset = maxSAPosY;
    // res.offset.y = (res.mask == (RCR_HIT_STEP_Y | RCR_HIT_POS_Y) ? maxSAPosY : maxPosY) + maxNegY;
}

inline bool CheckInverseStepAssistCollision(const AABB& a, const AABB& b)
{
    if (a.max.y < b.min.y || a.min.y > b.max.y) return false;
    return true;
}

inline bool CheckStepAssistCollision(float stepAssist, const AABB& a, const AABB& b, CollisionResult& res)
{
    if (a.max.y < b.min.y || a.min.y > b.max.y) return false;
    float overlapY = std::min(a.max.y, b.max.y) - std::max(a.min.y, b.min.y);
    if (overlapY < stepAssist && (a.min.y + a.max.y) * 0.5f > (b.min.y + b.max.y) * 0.5f)
    {
        res.penetration = overlapY + COLLISION_EPSILON;
        res.type = COLLISION_TYPE_STEP_Y;
    }
    return true;
}

inline bool CheckCollision(const AABB& a, const AABB& b, CollisionResult& res)
{
    if (a.max.x < b.min.x || a.min.x > b.max.x) return false;
    if (a.max.y < b.min.y || a.min.y > b.max.y) return false;
    if (a.max.z < b.min.z || a.min.z > b.max.z) return false;

    float overlapX = std::min(a.max.x, b.max.x) - std::max(a.min.x, b.min.x);
    float overlapY = std::min(a.max.y, b.max.y) - std::max(a.min.y, b.min.y);
    float overlapZ = std::min(a.max.z, b.max.z) - std::max(a.min.z, b.min.z);

    // Find the axis of shallowest penetration
    if (overlapY < overlapX && overlapY < overlapZ)
    {
        // Y-axis has the smallest overlap
        res.penetration = overlapY + COLLISION_EPSILON;

        float centerA_y = (a.min.y + a.max.y) * 0.5f;
        float centerB_y = (b.min.y + b.max.y) * 0.5f;

        res.type = (centerA_y < centerB_y) ? COLLISION_TYPE_NEG_Y : COLLISION_TYPE_POS_Y;
    }
    else if (overlapX < overlapZ)
    {
        // X-axis has the smallest overlap
        res.penetration = overlapX + COLLISION_EPSILON;

        float centerA_x = (a.min.x + a.max.x) * 0.5f;
        float centerB_x = (b.min.x + b.max.x) * 0.5f;

        res.type = (centerA_x < centerB_x) ? COLLISION_TYPE_NEG_X : COLLISION_TYPE_POS_X;
    }
    else
    {
        // Z-axis has the smallest overlap
        res.penetration = overlapZ + COLLISION_EPSILON;

        float centerA_z = (a.min.z + a.max.z) * 0.5f;
        float centerB_z = (b.min.z + b.max.z) * 0.5f;

        res.type = (centerA_z < centerB_z) ? COLLISION_TYPE_NEG_Z : COLLISION_TYPE_POS_Z;
    }
    return true;
}
}