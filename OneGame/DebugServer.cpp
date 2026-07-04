#include "Server.hpp"

namespace OneGame
{
    void DebugServer::Initialize(AppContext& ctx)
    {
        m_gameWorld.CreateTerrain();
        m_gameWorld.Register<Terrain::TerrainService>();
    }
    
    void DebugServer::Enter(AppContext& ctx)
    {
        ServerArgs _args{};
        auto args = ctx.sceneArgs.try_cast<ServerArgs>();
        if (!args) args = &_args;
        m_server.Initialize(args->port, args->maxClients);
    }
    
    void DebugServer::Update(AppContext& ctx, const FrameData& frame)
    {
        m_server.Poll(ctx.events);
    }
}