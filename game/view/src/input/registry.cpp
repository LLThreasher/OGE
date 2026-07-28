#include "game/input/input_source.hpp"

namespace game::input
{
void RegisterInputSources(AnythingFactory& af)
{
    af.RegisterABC<InputSource>();
    af.RegisterDerived<InputSource, UIDragInput>();
    af.RegisterDerived<InputSource, WidgetInput>();
    af.RegisterDerived<InputSource, KeyMouseInput>();
}
}  // namespace game::input
