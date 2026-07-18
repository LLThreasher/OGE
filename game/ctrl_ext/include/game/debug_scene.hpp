#include "game/scene.hpp"
#include "game/sim/subsystem.hpp"
#include "game/view/renderer.hpp"

namespace game
{
class DebugScene3 : public Scene
{
   public:
    DebugScene3(AnythingFactory& af) : Scene(af)
    {
        m_subsystems.AddStage<sim::SubsystemDebugText>();
        m_renderers.AddStage<view::DebugInfoRenderer>();
    }
};
}  // namespace game
