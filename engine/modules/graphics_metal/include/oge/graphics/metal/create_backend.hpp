#pragma once

#include <memory>

#include "oge/graphics/backend.hpp"

namespace oge::graphics::metal
{
std::unique_ptr<IGraphicsBackend> CreateMetalBackend();
}  // namespace oge::graphics::metal
