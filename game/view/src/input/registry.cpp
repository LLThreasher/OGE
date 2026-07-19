#include "game/input/input_source.hpp"

namespace game::input {
void RegisterInputSources(AnythingFactory &af)
{
    af.RegisterABC<InputSource>();
    af.RegisterDrived<InputSource, UIDragInput>();
    af.RegisterDrived<InputSource, WidgetInput>();
    af.RegisterDrived<InputSource, KeyMouseInput>();
}
}