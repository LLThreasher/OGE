#include "game/view/renderer.hpp"
#include "game/view/terrain/terrain_renderer.hpp"

namespace game::view
{
void RegisterRenderers(AnythingFactory& af)
{
    af.RegisterABC<Renderer>();
    af.RegisterDerived<Renderer, DebugInfoRenderer>();
    af.RegisterDerived<Renderer, TerrainRenderer>();
    af.RegisterDerived<Renderer, CameraRenderer>();
    af.RegisterDerived<Renderer, UIRenderer>();
    af.RegisterDerived<Renderer, GizmoRenderer>();
    af.RegisterDerived<Renderer, BlockHighlightRenderer>();
}
}  // namespace game::view
