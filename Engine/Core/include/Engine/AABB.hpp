#pragma once

#include <vector>

#include "Engine/Logger.hpp"
#include "Engine/Math.hpp"

namespace OneGame::Engine
{
struct AABB
{
    math::vec3 min;
    math::vec3 max;

    AABB operator+(const math::vec3& other) const noexcept { return {min + other, max + other}; }
};

}  // namespace OneGame::Engine