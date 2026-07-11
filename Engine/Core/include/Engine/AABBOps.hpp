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
    std::array<short, 4> resolvedIndices = {-1, -1, -1, -1};
    uint32_t mask = 0;
};

inline void ResolveCollisionsY(const std::vector<CollisionResult>& collisions, ResolveCollisionResult& res)
{
    if (collisions.empty())
    {
        return;
    }

    // Separate tracking for positive (+) and negative (-) pushes on each axis
    float maxPosY = 0.0f;
    float maxNegY = 0.0f;
    float maxSAPosY = 0.0f;

    short maxPosYIdx = -1;
    short maxNegYIdx = -1;
    short maxSAPosYIdx = -1;

    for (int i = 0; i < collisions.size(); ++i)
    {
        auto& col = collisions[i];
        auto& normal = COLLISION_NORMALS[col.type];
        float pushY = normal.y * col.penetration;

        // Y-Axis sorting by sign
        if (col.type == COLLISION_TYPE_STEP_Y)
        {
            if (maxSAPosY < pushY + COLLISION_EPSILON)
            {
                maxSAPosY = pushY + COLLISION_EPSILON;
                maxSAPosYIdx = i;
            }
            res.mask |= RCR_HIT_STEP_Y;
        }
        else if (pushY > 0.0f)
        {
            if (maxPosY < pushY + COLLISION_EPSILON)
            {
                maxPosY = pushY + COLLISION_EPSILON;
                maxPosYIdx = i;
            }
            res.mask |= RCR_HIT_POS_Y;
        }
        else if (pushY < 0.0f)
        {
            if (maxNegY > pushY - COLLISION_EPSILON)
            {
                maxNegY = pushY - COLLISION_EPSILON;
                maxNegYIdx = i;
            }
            res.mask |= RCR_HIT_NEG_Y;
        }
    }

    // Combine opposing forces by adding them together.
    // If stuck between two walls: (+2.0) + (-2.0) = 0.0 total movement. The player stays in the center.
    res.offset.y = maxPosY + maxNegY;
    res.stepOffset = maxSAPosY;

    res.resolvedIndices[1] = -maxNegY > maxPosY ? maxNegYIdx : maxPosYIdx;
    res.resolvedIndices[3] = maxSAPosYIdx;
    // res.offset.y = (res.mask == (RCR_HIT_STEP_Y | RCR_HIT_POS_Y) ? maxSAPosY : maxPosY) + maxNegY;
}

inline void ResolveCollisionsX(const std::vector<CollisionResult>& collisions, ResolveCollisionResult& res)
{
    if (collisions.empty())
    {
        res = {};
        return;
    }

    // Separate tracking for positive (+) and negative (-) pushes on each axis
    float maxPosX = 0.0f;
    float maxNegX = 0.0f;
    float maxSAPosY = 0.0f;

    short maxPosXIdx = -1;
    short maxNegXIdx = -1;
    short maxSAPosYIdx = -1;

    for (int i = 0; i < collisions.size(); ++i)
    {
        auto& col = collisions[i];
        auto& normal = COLLISION_NORMALS[col.type];
        float pushX = normal.x * col.penetration;
        float pushY = normal.y * col.penetration;

        // X-Axis sorting by sign
        if (pushX > 0.0f)
        {
            if (maxPosX < pushX - COLLISION_EPSILON)
            {
                maxPosX = pushX;
                maxPosXIdx = i;
            }
            res.mask |= RCR_HIT_POS_X;
        }
        else if (pushX < 0.0f)
        {
            if (maxNegX > pushX + COLLISION_EPSILON)
            {
                maxNegX = pushX;
                maxNegXIdx = i;
            }
            res.mask |= RCR_HIT_NEG_X;
        }

        // Y-Axis sorting by sign
        if (col.type == COLLISION_TYPE_STEP_Y)
        {
            if (maxSAPosY < pushY + COLLISION_EPSILON)
            {
                maxSAPosY = pushY + COLLISION_EPSILON * 2.5f;
                maxSAPosYIdx = i;
            }
            res.mask |= RCR_HIT_STEP_Y;
        }
    }

    // Combine opposing forces by adding them together.
    // If stuck between two walls: (+2.0) + (-2.0) = 0.0 total movement. The player stays in the center.
    res.offset.x = maxPosX + maxNegX;
    res.stepOffset = maxSAPosY;

    res.resolvedIndices[0] = -maxNegX > maxPosX ? maxNegXIdx : maxPosXIdx;
    res.resolvedIndices[3] = maxSAPosYIdx;
    // res.offset.y = (res.mask == (RCR_HIT_STEP_Y | RCR_HIT_POS_Y) ? maxSAPosY : maxPosY) + maxNegY;
}

inline void ResolveCollisionsZ(const std::vector<CollisionResult>& collisions, ResolveCollisionResult& res)
{
    if (collisions.empty())
    {
        res = {};
        return;
    }

    // Separate tracking for positive (+) and negative (-) pushes on each axis
    float maxPosZ = 0.0f;
    float maxNegZ = 0.0f;
    float maxSAPosY = 0.0f;

    short maxPosZIdx = -1;
    short maxNegZIdx = -1;
    short maxSAPosYIdx = -1;

    for (int i = 0; i < collisions.size(); ++i)
    {
        auto& col = collisions[i];
        auto& normal = COLLISION_NORMALS[col.type];
        float pushZ = normal.z * col.penetration;
        float pushY = normal.y * col.penetration;

        // Z-Axis sorting by sign
        if (pushZ > 0.0f)
        {
            if (maxPosZ < pushZ - COLLISION_EPSILON)
            {
                maxPosZ = pushZ;
                maxPosZIdx = i;
            }
            res.mask |= RCR_HIT_POS_Z;
        }
        else if (pushZ < 0.0f)
        {
            if (maxNegZ > pushZ + COLLISION_EPSILON)
            {
                maxNegZ = pushZ;
                maxNegZIdx = i;
            }
            res.mask |= RCR_HIT_NEG_Z;
        }

        // Y-Axis sorting by sign
        if (col.type == COLLISION_TYPE_STEP_Y)
        {
            if (maxSAPosY < pushY + COLLISION_EPSILON)
            {
                maxSAPosY = pushY + COLLISION_EPSILON * 2.5f;
                maxSAPosYIdx = i;
            }
            res.mask |= RCR_HIT_STEP_Y;
        }
    }

    // Combine opposing forces by adding them together.
    // If stuck between two walls: (+2.0) + (-2.0) = 0.0 total movement. The player stays in the center.
    res.offset.z = maxPosZ + maxNegZ;
    res.stepOffset = maxSAPosY;

    res.resolvedIndices[2] = -maxNegZ > maxPosZ ? maxNegZIdx : maxPosZIdx;
    res.resolvedIndices[3] = maxSAPosYIdx;
    // res.offset.y = (res.mask == (RCR_HIT_STEP_Y | RCR_HIT_POS_Y) ? maxSAPosY : maxPosY) + maxNegY;
}

inline void ResolveCollisionsXZ(const std::vector<CollisionResult>& collisions, ResolveCollisionResult& res)
{
    if (collisions.empty())
    {
        res = {};
        return;
    }

    // Separate tracking for positive (+) and negative (-) pushes on each axis
    float maxPosX = 0.0f;
    float maxNegX = 0.0f;
    float maxPosZ = 0.0f;
    float maxNegZ = 0.0f;

    short maxPosXIdx = -1;
    short maxNegXIdx = -1;
    short maxPosZIdx = -1;
    short maxNegZIdx = -1;

    for (int i = 0; i < collisions.size(); ++i)
    {
        auto& col = collisions[i];
        auto& normal = COLLISION_NORMALS[col.type];
        float pushX = normal.x * col.penetration;
        float pushZ = normal.z * col.penetration;

        // X-Axis sorting by sign
        if (pushX > 0.0f)
        {
            if (maxPosX < pushX - COLLISION_EPSILON)
            {
                maxPosX = pushX;
                maxPosXIdx = i;
            }
            res.mask |= RCR_HIT_POS_X;
        }
        else if (pushX < 0.0f)
        {
            if (maxNegX > pushX + COLLISION_EPSILON)
            {
                maxNegX = pushX;
                maxNegXIdx = i;
            }
            res.mask |= RCR_HIT_NEG_X;
        }

        // Z-Axis sorting by sign
        if (pushZ > 0.0f)
        {
            if (maxPosZ < pushZ - COLLISION_EPSILON)
            {
                maxPosZ = pushZ;
                maxPosZIdx = i;
            }
            res.mask |= RCR_HIT_POS_Z;
        }
        else if (pushZ < 0.0f)
        {
            if (maxNegZ > pushZ + COLLISION_EPSILON)
            {
                maxNegZ = pushZ;
                maxNegZIdx = i;
            }
            res.mask |= RCR_HIT_NEG_Z;
        }
    }

    // Combine opposing forces by adding them together.
    // If stuck between two walls: (+2.0) + (-2.0) = 0.0 total movement. The player stays in the center.
    res.offset.x = maxPosX + maxNegX;
    res.offset.z = maxPosZ + maxNegZ;

    res.resolvedIndices[0] = -maxNegX > maxPosX ? maxNegXIdx : maxPosXIdx;
    res.resolvedIndices[2] = -maxNegZ > maxPosZ ? maxNegZIdx : maxPosZIdx;
    // res.offset.y = (res.mask == (RCR_HIT_STEP_Y | RCR_HIT_POS_Y) ? maxSAPosY : maxPosY) + maxNegY;
}

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

    short maxPosXIdx = -1;
    short maxNegXIdx = -1;
    short maxPosYIdx = -1;
    short maxNegYIdx = -1;
    short maxPosZIdx = -1;
    short maxNegZIdx = -1;
    short maxSAPosYIdx = -1;

    for (int i = 0; i < collisions.size(); ++i)
    {
        auto& col = collisions[i];
        auto& normal = COLLISION_NORMALS[col.type];
        float pushX = normal.x * col.penetration;
        float pushY = normal.y * col.penetration;
        float pushZ = normal.z * col.penetration;

        // X-Axis sorting by sign
        if (pushX > 0.0f)
        {
            if (maxPosX < pushX + COLLISION_EPSILON)
            {
                maxPosX = pushX + COLLISION_EPSILON;
                maxPosXIdx = i;
            }
            res.mask |= RCR_HIT_POS_X;
        }
        else if (pushX < 0.0f)
        {
            if (maxNegX > pushX - COLLISION_EPSILON)
            {
                maxNegX = pushX - COLLISION_EPSILON;
                maxNegXIdx = i;
            }
            res.mask |= RCR_HIT_NEG_X;
        }

        // Y-Axis sorting by sign
        if (col.type == COLLISION_TYPE_STEP_Y)
        {
            if (maxSAPosY < pushY + COLLISION_EPSILON)
            {
                maxSAPosY = pushY + COLLISION_EPSILON;
                maxSAPosYIdx = i;
            }
            res.mask |= RCR_HIT_STEP_Y;
        }
        else if (pushY > 0.0f)
        {
            if (maxPosY < pushY + COLLISION_EPSILON)
            {
                maxPosY = pushY + COLLISION_EPSILON;
                maxPosYIdx = i;
            }
            res.mask |= RCR_HIT_POS_Y;
        }
        else if (pushY < 0.0f)
        {
            if (maxNegY > pushY - COLLISION_EPSILON)
            {
                maxNegY = pushY - COLLISION_EPSILON;
                maxNegYIdx = i;
            }
            res.mask |= RCR_HIT_NEG_Y;
        }

        // Z-Axis sorting by sign
        if (pushZ > 0.0f)
        {
            if (maxPosZ < pushZ + COLLISION_EPSILON)
            {
                maxPosZ = pushZ + COLLISION_EPSILON;
                maxPosZIdx = i;
            }
            res.mask |= RCR_HIT_POS_Z;
        }
        else if (pushZ < 0.0f)
        {
            if (maxNegZ > pushZ - COLLISION_EPSILON)
            {
                maxNegZ = pushZ - COLLISION_EPSILON;
                maxNegZIdx = i;
            }
            res.mask |= RCR_HIT_NEG_Z;
        }
    }

    // Combine opposing forces by adding them together.
    // If stuck between two walls: (+2.0) + (-2.0) = 0.0 total movement. The player stays in the center.
    res.offset.x = maxPosX + maxNegX;
    res.offset.z = maxPosZ + maxNegZ;
    res.offset.y = maxPosY + maxNegY;
    res.stepOffset = maxSAPosY;

    res.resolvedIndices[0] = -maxNegX > maxPosX ? maxNegXIdx : maxPosXIdx;
    res.resolvedIndices[1] = -maxNegY > maxPosY ? maxNegYIdx : maxPosYIdx;
    res.resolvedIndices[2] = -maxNegZ > maxPosZ ? maxNegZIdx : maxPosZIdx;
    res.resolvedIndices[3] = maxSAPosYIdx;
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
    if (overlapY < COLLISION_EPSILON) return false;
    if (overlapY < stepAssist && (a.min.y + a.max.y) * 0.5f > (b.min.y + b.max.y) * 0.5f)
    {
        res.penetration = overlapY + COLLISION_EPSILON;
        res.type = COLLISION_TYPE_STEP_Y;
    }
    return true;
}

inline bool CheckCollisionY(const AABB& a, const AABB& b, CollisionResult& res)
{
    if (a.max.y < b.min.y || a.min.y > b.max.y) return false;

    float overlapY = std::min(a.max.y, b.max.y) - std::max(a.min.y, b.min.y);

    // Y-axis has the smallest overlap
    res.penetration = overlapY + COLLISION_EPSILON;

    float centerA_y = (a.min.y + a.max.y) * 0.5f;
    float centerB_y = (b.min.y + b.max.y) * 0.5f;

    res.type = (centerA_y < centerB_y) ? COLLISION_TYPE_NEG_Y : COLLISION_TYPE_POS_Y;

    return true;
}

inline bool CheckCollisionX(const AABB& a, const AABB& b, CollisionResult& res)
{
    if (a.max.x < b.min.x || a.min.x > b.max.x) return false;

    float overlapX = std::min(a.max.x, b.max.x) - std::max(a.min.x, b.min.x);

    // X-axis has the smallest overlap
    res.penetration = overlapX + COLLISION_EPSILON;

    float centerA_x = (a.min.x + a.max.x) * 0.5f;
    float centerB_x = (b.min.x + b.max.x) * 0.5f;

    res.type = (centerA_x < centerB_x) ? COLLISION_TYPE_NEG_X : COLLISION_TYPE_POS_X;
    return true;
}

inline bool CheckCollisionZ(const AABB& a, const AABB& b, CollisionResult& res)
{
    if (a.max.z < b.min.z || a.min.z > b.max.z) return false;

    float overlapZ = std::min(a.max.z, b.max.z) - std::max(a.min.z, b.min.z);

    // Z-axis has the smallest overlap
    res.penetration = overlapZ + COLLISION_EPSILON;

    float centerA_z = (a.min.z + a.max.z) * 0.5f;
    float centerB_z = (b.min.z + b.max.z) * 0.5f;

    res.type = (centerA_z < centerB_z) ? COLLISION_TYPE_NEG_Z : COLLISION_TYPE_POS_Z;
    return true;
}

inline bool CheckCollisionXZ(const AABB& a, const AABB& b, CollisionResult& res)
{
    if (a.max.x < b.min.x || a.min.x > b.max.x) return false;
    if (a.max.z < b.min.z || a.min.z > b.max.z) return false;

    float overlapX = std::min(a.max.x, b.max.x) - std::max(a.min.x, b.min.x);
    float overlapZ = std::min(a.max.z, b.max.z) - std::max(a.min.z, b.min.z);

    if (overlapX < overlapZ)
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