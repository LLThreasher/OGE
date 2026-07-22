#include <concepts>
#include <type_traits>
#include "game/sim/registry.hpp"
#include "game/components.hpp"
#include "game/sim/subsystem.hpp"
#include "game/sim/terrain/subsystem_terrain.hpp"
#include "game/sim/subsystem_physics.hpp"

#define RR(SysName) R<SysName>(af)
#define RRU(SysName) R<SysName<UpdateType::Realtime>>(af);R<SysName<UpdateType::FixedStep>>(af);

namespace game::sim
{
template<typename T>
static void R(AnythingFactory& af)
{
    af.RegisterDrived<Subsystem, T>();
}

void RegisterSubsystems(AnythingFactory& af)
{
    af.RegisterABC<Subsystem>();

    RR(SubsystemDebugText);
    RR(SubsystemTerrain);
    RRU(SubsystemCreature);
    RRU(SubsystemPhysics);
    RRU(SubsystemPlayer);
}
}  // namespace game::sim
