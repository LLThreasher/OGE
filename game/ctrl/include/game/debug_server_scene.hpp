#include "game/app_context.hpp"
#include "game/scene.hpp"
#include "oge/runtime/net_server.hpp"

namespace game
{

class DebugServerScene : public Scene
{
    oge::runtime::NetServer m_netServer = {1 * 1024 * 1024}; // 1 MB send buffer
   public:
    DebugServerScene(AppContext ctx) : Scene(std::move(ctx)) {}
};
}  // namespace game