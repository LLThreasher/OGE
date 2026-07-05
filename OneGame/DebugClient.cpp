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
    m_client.Connect(args->ip.c_str(), args->port, args->timeout);
}

void DebugClient::Update(PresentationContext& context, const FrameInputData& frame, FrameOutputData& frameOut)
{
    if (m_client.Status() == ClientStatus::Disconnected || m_client.Status() == ClientStatus::ConnectFailed) return;
    m_client.Poll(context.events, frame.dt);
    if (m_client.Status() == ClientStatus::Disconnecting || m_client.Status() == ClientStatus::Connecting) return;
}
}