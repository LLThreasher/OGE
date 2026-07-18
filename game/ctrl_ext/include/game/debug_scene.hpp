#include "game/scene.hpp"
#include "game/sim/subsystem.hpp"

namespace game
{
class DebugScene3 : public Scene
{
   public:
    DebugScene3(AnythingFactory& af) : Scene(af) { m_subsystems.AddStage<sim::SubsystemDebugText>(); }
};
}  // namespace game
