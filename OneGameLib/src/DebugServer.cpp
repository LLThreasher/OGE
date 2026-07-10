#include "OneGame/Server.hpp"

namespace OneGame
{
void DebugServer::Initialize(AppContext& ctx)
{
    using namespace ECS;
    m_gameWorld.CreateServer();
    m_gameWorld.CreateTerrain();
    m_gameWorld.Register<Terrain::TerrainService>();
    m_gameWorld.Register<SubsystemPlayer>();
}

void DebugServer::Enter(AppContext& ctx)
{
    ServerArgs _args{};
    auto args = ctx.sceneArgs.try_cast<ServerArgs>();
    if (!args) args = &_args;
    m_gameWorld.Get().ctx().get<NetServer>().Initialize(args->port, args->maxClients);
    m_server.Initialize(m_gameWorld.Get(), ctx.events);
}

void DebugServer::Update(AppContext& ctx, const FrameData& frame)
{
    m_gameWorld.Get().ctx().get<NetServer>().Poll(ctx.events);
    m_server.Update(m_gameWorld.Get());
}

void DebugServer::onServerReceiveConnect(OnServerReceiveConnect&) {}
void DebugServer::onServerReceiveDisconnect(OnServerReceiveDisconnect&) {}
void DebugServer::onServerReceivePacket(OnServerReceivePacket&) {}
}  // namespace OneGame