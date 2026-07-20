#include "game/sim/subsystem.hpp"
#include "game/sim/terrain/subsystem_terrain.hpp"

namespace game::sim
{
void RegisterSubsystems(AnythingFactory& af)
{
    af.RegisterABC<Subsystem>();
    af.RegisterDrived<Subsystem, SubsystemDebugText>();
    af.RegisterDrived<Subsystem, SubsystemTerrain>();
    af.RegisterDrived<Subsystem, SubsystemPlayer>();
}
} // namespace game::sim
