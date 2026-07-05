#pragma once
#include "Engine/IScene.hpp"
#include "Engine/ECS/GameWorld.hpp"
#include "Engine/ECS/NetServer.hpp"
#include "Engine/ECS/GameNetSerializer.hpp"

namespace OneGame
{
using namespace Engine;

struct ServerArgs
{
    uint16_t port = 25567;
    size_t maxClients = 10;
};

class DebugServer final : public Scene<AppContext, const FrameData>
{
public:
    void Initialize(AppContext& ctx) override;
    void Enter(AppContext& ctx) override;
    void Update(AppContext& ctx, const FrameData& frame) override;
private:
    void onServerReceiveConnect(OnServerReceiveConnect&);
    void onServerReceiveDisconnect(OnServerReceiveDisconnect&);
    void onServerReceivePacket(OnServerReceivePacket&);
    
    ECS::GameWorld m_gameWorld;
    GameServer m_server;
};
} // namespace OneGame
