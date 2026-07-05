#include "Client.hpp"

namespace OneGame
{
void DebugClient::Initialize(PresentationContext& context)
{
    using namespace ECS;
    m_gameWorld.Register<SubsystemUI>();
    m_gameWorld.Register<SubsystemPlayerInput>();
    m_gameWorld.Register<SubsystemPlayer>();

    m_gameRenderer.Register<DebugInfoRenderer>();
    m_gameRenderer.Register<Terrain::TerrainRenderer>();
    m_gameRenderer.Register<UIRenderer>();
    m_gameRenderer.Register<CameraRenderer>();
    m_gameRenderer.Register<PlayerInputRenderer>();
}

void DebugClient::Enter(PresentationContext& context)
{
    ClientArgs _args{};
    auto args = context.sceneArgs.try_cast<ClientArgs>();
    if (!args) {args = &_args;}
    m_client.Initialize();
    m_client.Connect("127.0.0.1", args->port, args->timeout);
}

void DebugClient::Update(PresentationContext& context, const FrameInputData& frame, FrameOutputData& frameOut)
{
    m_client.Poll(context.events);
}
}