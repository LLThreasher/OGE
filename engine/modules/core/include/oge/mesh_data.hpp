#pragma once

#include <vector>

#include "oge/math.hpp"

namespace oge
{
struct MeshData
{
    std::vector<math::vec3> positions;
    std::vector<math::vec2> uvs;
    std::vector<uint32_t> indices;
};
}  // namespace oge
