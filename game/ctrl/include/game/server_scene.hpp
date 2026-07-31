#include <vector>

#include "game/app_context.hpp"
#include "game/components.hpp"
#include "game/replication_registry.hpp"
#include "game/scene.hpp"
#include "oge/log.hpp"
#include "oge/runtime/net_packet_sender.hpp"
#include "oge/runtime/net_server.hpp"
#include "uuid.h"

namespace game
{
using oge::runtime::NetServer;
using oge::runtime::OnServerReceiveConnect;
using oge::runtime::OnServerReceiveDisconnect;
using oge::runtime::OnServerReceivePacket;

class DebugServerScene final : public Scene
{
    entt::dispatcher m_serverEventDispatcher;
    NetServer& m_netServer;
    ENetPeer* m_pendingConnection = nullptr;
    std::vector<PlayerInfo> m_playerEntries;

    void onServerRecieveConnect(OnServerReceiveConnect c)
    {
        LOG_INFO("connecting {}", c.peerId);
        if (m_pendingConnection)
        {
            m_netServer.Disconnect(c.peer);
            LOG_INFO("drop peer connection due to flow ctrl: {}",
                     (void*)c.peer);
        }
        m_pendingConnection = c.peer;
    }

    void onServerReceiveDisconnect(OnServerReceiveDisconnect c)
    {
        LOG_INFO("disconnecting {}", c.peerId);
        if (m_playerEntries.size() > c.peerId)
        {
            auto uuid = uuids::uuid{m_playerEntries[c.peerId].uuid};
            LOG_INFO("found uuid {}", uuids::to_string(uuid));
            if (!uuid.is_nil())
            {
                LOG_INFO("destroy player enitty: {}",
                         uuids::to_string(uuid));
                ComponentPlayer::DestroyPlayer(m_world,
                                               m_playerEntries[c.peerId]);
            }
        }
    }

    void onServerReceivePacket(OnServerReceivePacket p)
    {
        if (p.peer == m_pendingConnection)
        {
            m_pendingConnection = nullptr;
            auto playerInfo = p.data->Read<PlayerInfo>();
            ComponentPlayer::CreatePlayer(m_world, playerInfo);
            m_playerEntries.resize(
                math::max(p.peerId + 1, (uint32_t)m_playerEntries.size()));
            m_playerEntries[p.peerId] = playerInfo;
            LOG_INFO("create player enitty: {} for conn({})",
                     uuids::to_string(uuids::uuid{m_playerEntries[p.peerId].uuid}), p.peerId);
        }
    }

   public:
    DebugServerScene(const Def& def)
        : Scene(def), m_netServer(m_ctx.any_ctx.Emplace<NetServer>())
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
        m_netServer.Initialize(port, maxClients, 3);
        m_serverEventDispatcher.sink<OnServerReceiveConnect>()
            .connect<&DebugServerScene::onServerRecieveConnect>(this);
        m_serverEventDispatcher.sink<OnServerReceiveDisconnect>()
            .connect<&DebugServerScene::onServerReceiveDisconnect>(this);
        m_serverEventDispatcher.sink<OnServerReceivePacket>()
            .connect<&DebugServerScene::onServerReceivePacket>(this);
    }

    ~DebugServerScene()
    {
        m_netServer.Shutdown();
        m_ctx.any_ctx.Erase<oge::runtime::NetServer>();
    }

    void Update(Frame f, SceneContext sctx) override
    {
        m_netServer.Poll(m_serverEventDispatcher);
        assert(m_serverEventDispatcher.size() == 0);
        Scene::Update(f, sctx);
    }
};
}  // namespace game

namespace oge::runtime
{
template <>
struct TypeName<game::DebugServerScene>
{
    static consteval std::string_view Get()
    {
        return "core::DebugServerScene";
    }
};
}  // namespace oge::runtime
