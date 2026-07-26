#include "game/app_context.hpp"
#include "game/net_events.hpp"
#include "game/scene.hpp"
#include "oge/runtime/net_server.hpp"

namespace game
{

class DebugServerScene : public Scene
{
    entt::dispatcher m_serverEventDispatcher;
    oge::runtime::NetServer m_netServer;
    events::ServerNetTransportationLayer m_netTransLayer;

   public:
    DebugServerScene(const Def& def)
        : Scene(def), m_netTransLayer(m_world, m_serverEventDispatcher)
    {
    }

    void Update(Frame f, SceneContext sctx) override
    {
        m_netServer.Poll(m_serverEventDispatcher);
        assert(m_serverEventDispatcher.size() == 0);
        Scene::Update(f, sctx);
        m_netTransLayer.PostUpdate(m_netServer, m_world);
    }
};
}  // namespace game
