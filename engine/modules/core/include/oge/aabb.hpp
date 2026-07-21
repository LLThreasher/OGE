#pragma once

#include "oge/math.hpp"

namespace oge
{
struct AABB
{
    math::vec3 min;
    math::vec3 max;

    AABB operator+(const math::vec3& other) const noexcept { return {min + other, max + other}; }
};

inline bool operator==(const AABB& a, const AABB& b) noexcept { return a.min == b.min && a.max == b.max; }

inline bool operator!=(const AABB& a, const AABB& b) noexcept { return !(a == b); }

}  // namespace oge