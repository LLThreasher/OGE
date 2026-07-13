#pragma once

#include "Engine/AABB.hpp"

namespace OneGame::Engine
{

constexpr int32_t COLLISION_TYPE_POS_X = 0;
constexpr int32_t COLLISION_TYPE_NEG_X = 1;
constexpr int32_t COLLISION_TYPE_POS_Y = 2;
constexpr int32_t COLLISION_TYPE_NEG_Y = 3;
constexpr int32_t COLLISION_TYPE_POS_Z = 4;
constexpr int32_t COLLISION_TYPE_NEG_Z = 5;
constexpr int32_t COLLISION_TYPE_STEP_Y = 6;

const math::vec3 COLLISION_NORMALS[] = {
    math::vec3{1.f, 0.f, 0.f}, math::vec3{-1.f, 0.f, 0.f}, math::vec3{0.f, 1.f, 0.f}, math::vec3{0.f, -1.f, 0.f},
    math::vec3{0.f, 0.f, 1.f}, math::vec3{0.f, 0.f, -1.f}, math::vec3{0.f, 1.f, 0.f}, math::vec3{0.f, -1.f, 0.f},
};

constexpr float COLLISION_EPSILON = 0.0001f;

struct CollisionResult2
{
    int32_t type;
    math::vec3 effectiveOffset = {};
};

template <size_t dim>
inline bool CheckCollision(const AABB& a, const AABB& b, float& offset, int32_t& collisionType)
{
    const float a_max = a.max[dim] + COLLISION_EPSILON;
    const float a_min = a.min[dim] - COLLISION_EPSILON;
    const float b_max = b.max[dim] + COLLISION_EPSILON;
    const float b_min = b.min[dim] - COLLISION_EPSILON;

    auto beginOffset = offset;
    if (offset > 0.f)
    {
        float maxOffset = math::max(0.f, b_min - a_max - COLLISION_EPSILON);
        assert(a_max <= b_min || maxOffset == 0.f);
        if (offset > maxOffset)
        {
            offset = maxOffset;
            collisionType = dim * 2 + 1;
            assert(offset <= beginOffset);
            return true;
        }
    }
    else if (offset < 0.f)
    {
        float minOffset = math::min(0.f, b_max - a_min + COLLISION_EPSILON);
        assert(b_max <= a_min || minOffset == 0.f);
        if (offset < minOffset)
        {
            offset = minOffset;
            collisionType = dim * 2;
            assert(offset >= beginOffset);
            return true;
        }
    }

    assert(offset == beginOffset);
    return false;
}

template <size_t collisionType>
inline bool CheckCollisionRejection(const AABB& a, const AABB& b, float& penetration)
{
    constexpr size_t dim = collisionType / 3;
    const float a_max = a.max[dim] + COLLISION_EPSILON;
    const float a_min = a.min[dim] - COLLISION_EPSILON;
    const float b_max = b.max[dim] + COLLISION_EPSILON;
    const float b_min = b.min[dim] - COLLISION_EPSILON;

    if constexpr ((collisionType % 2) == 0)
    {
        assert(a_min > b_max);
        if (a_min + penetration + COLLISION_EPSILON < b_max)
        {
            penetration = b_max - a_min;
            return true;
        }
    }
    if constexpr ((collisionType % 2) == 1)
    {
        if (a_max - penetration > b_min)
        {
            penetration = a_max - b_min;
            return true;
        }
    }

    return false;
}

inline bool CheckOverlap(const AABB& a, const AABB& b)
{
    if (a.max.x < b.min.x || a.min.x > b.max.x) return false;
    if (a.max.y < b.min.y || a.min.y > b.max.y) return false;
    if (a.max.z < b.min.z || a.min.z > b.max.z) return false;
    return true;
}
}