#include "oge/graphics/metal/create_backend.hpp"

#include "metal.hpp"

namespace oge::graphics::metal
{
std::unique_ptr<IGraphicsBackend> CreateMetalBackend()
{
    return std::make_unique<MetalBackend>();
}
}  // namespace oge::graphics::metal
