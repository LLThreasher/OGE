#include "game/app_context.hpp"
#include "game/net_events.hpp"
#include "game/scene.hpp"
#include "oge/runtime/net_server.hpp"

namespace game
{

class DebugServerScene final : public Scene
{
    entt::dispatcher m_serverEventDispatcher;
    oge::runtime::NetServer& m_netServer;
    events::ServerNetObjectTransportationLayer m_netTransLayer;

   public:
    DebugServerScene(const Def& def)
        : Scene(def),
          m_netTransLayer(m_world, m_serverEventDispatcher),
          m_netServer(m_ctx.any_ctx.Emplace<oge::runtime::NetServer>())
    {
        uint16_t port = 23400;
        size_t maxClients = 20;
        {
            auto it = def.args.find("port");
            if (it != def.args.end()) port = std::get<int64_t>(it->second);
        }
        {
            auto it = def.args.find("maxClients");
            if (it != def.args.end())
                maxClients = std::get<int64_t>(it->second);
        }
        m_netServer.Initialize(port, maxClients);
    }

    ~DebugServerScene()
    {
        m_netServer.Shutdown();
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

namespace oge::runtime {
template<>
struct TypeName<game::DebugServerScene>
{
    static consteval std::string_view Get()
    {
        return "core::DebugServerScene";
    }
};
}
