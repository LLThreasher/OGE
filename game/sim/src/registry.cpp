#include "game/sim/subsystem.hpp"
#include "game/sim/terrain/subsystem_terrain.hpp"

#define R(SysName) af.RegisterDrived<Subsystem, SysName>()

namespace game::sim
{
void RegisterSubsystems(AnythingFactory& af)
{
    af.RegisterABC<Subsystem>();

    R(SubsystemDebugText);
    R(SubsystemTerrain);
    R(SubsystemPlayer);
    R(SubsystemCreature);
    R(SubsystemPhysics);
}
}  // namespace game::sim
