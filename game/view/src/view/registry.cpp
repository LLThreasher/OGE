#include "game/view/renderer.hpp"
#include "game/view/terrain/terrain_renderer.hpp"

namespace game::view
{
void RegisterRenderers(AnythingFactory& af)
{
    af.RegisterABC<Renderer>();
    af.RegisterDrived<Renderer, DebugInfoRenderer>();
    af.RegisterDrived<Renderer, TerrainRenderer>();
}
} // namespace game::view
