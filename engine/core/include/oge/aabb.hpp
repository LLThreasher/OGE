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

}  // namespace OneGame::Engine