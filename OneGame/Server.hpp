#pragma once
#include "Engine/IScene.hpp"
#include "Engine/ECS/GameWorld.hpp"
#include "Engine/ECS/NetServer.hpp"

namespace OneGame
{
using namespace Engine;

struct ServerArgs
{
    uint16_t port = 25567;
    size_t maxClients = 10;
};

class DebugServer : public Scene<AppContext, const FrameData>
{
public:
    virtual void Initialize(AppContext& ctx) override;
    virtual void Enter(AppContext& ctx) override;
    virtual void Update(AppContext& ctx, const FrameData& frame) override;
private:
    ECS::GameWorld m_gameWorld;
    NetServer m_server;
};
} // namespace OneGame
