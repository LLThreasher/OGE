#include <concepts>
#include <type_traits>
#include "game/sim/subsystem.hpp"
#include "game/sim/terrain/subsystem_terrain.hpp"
#include "game/sim/subsystem_physics.hpp"

#define RR(SysName) R<SysName>(af)

namespace game::sim
{
template<typename T>
static void R(AnythingFactory& af)
{
    if constexpr (std::derived_from<T, RealtimeSystem>) {
        af.RegisterDrived<RealtimeSystem, T>();
    }
    if constexpr (std::derived_from<T, Subsystem>) {
        af.RegisterDrived<Subsystem, T>();
    }
}

void RegisterSubsystems(AnythingFactory& af)
{
    af.RegisterABC<Subsystem>();
    af.RegisterABC<RealtimeSystem>();

    RR(SubsystemDebugText);
    RR(SubsystemTerrain);
    RR(SubsystemCreature);
    RR(SubsystemPhysics);
    RR(SubsystemPlayer);
}
}  // namespace game::sim
