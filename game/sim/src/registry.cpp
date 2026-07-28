#include "game/sim/registry.hpp"

#include <concepts>
#include <string_view>
#include <type_traits>

#include "game/components.hpp"
#include "game/sim/subsystem.hpp"
#include "game/sim/subsystem_physics.hpp"
#include "game/sim/terrain/subsystem_terrain.hpp"
#include "oge/runtime/typed_registry.hpp"

#define RR(SysName) R<SysName>(af)
#define RRU(SysName)                      \
    R<SysName<UpdateType::Realtime>>(af); \
    R<SysName<UpdateType::FixedStep>>(af);

namespace game::sim
{
template <typename T>
static void R(AnythingFactory& af)
{
    af.RegisterDerived<Subsystem, T>();
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
