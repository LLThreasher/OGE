#include "Client.hpp"
#include "Engine/ECS/GameNetSerializer.hpp"

namespace OneGame
{
void DebugClient::Initialize(PresentationContext& ctx)
{
    using namespace ECS;
    m_gameWorld.CreateClient();
    m_gameWorld.CreateTerrain();

    m_gameWorld.Register<SubsystemUI>();
    m_gameWorld.Register<SubsystemPlayerInput>();
    m_gameWorld.Register<SubsystemPlayer>();

    m_gameRenderer.Register<DebugInfoRenderer>();
    m_gameRenderer.Register<Terrain::TerrainRenderer>();
    m_gameRenderer.Register<UIRenderer>();
    m_gameRenderer.Register<CameraRenderer>();
    m_gameRenderer.Register<PlayerInputRenderer>();
}

void DebugClient::Enter(PresentationContext& ctx)
{
    ClientArgs _args{};
    auto args = ctx.sceneArgs.try_cast<ClientArgs>();
    if (!args) {args = &_args;}
    auto& client = m_gameWorld.Get().ctx().get<NetClient>();
    client.Initialize();
    client.Connect(args->ip.c_str(), args->port, args->timeout);

    ctx.events.sink<OnClientPacketReceived>().connect<&DebugClient::onPacketRecieved>(this);
    ctx.events.sink<OnClientConnected>().connect<&DebugClient::onClientConnected>(this);

    m_state = ClientState::WaitingConnect;
}

void DebugClient::onClientConnected(OnClientConnected&)
{
    m_state = ClientState::WaitingConfig;
}

void DebugClient::onPacketRecieved(OnClientPacketReceived& packet)
{
    Net::Buffer buf{packet.data, packet.length};
    if (m_state == ClientState::WaitingConfig)
    {
        m_networkPuller.HandleInitPacket(buf, m_gameWorld.Get());
        m_state = ClientState::RecievedConfig;
    }
    else if (m_state != ClientState::Disconnected)
    {
        m_networkPuller.HandlePacket(buf, m_gameWorld.Get());
    }
}

void DebugClient::Update(PresentationContext& ctx, const FrameInputData& frame, FrameOutputData& frameOut)
{
    auto& client = m_gameWorld.Get().ctx().get<NetClient>();
    if (client.Status() == ClientStatus::Disconnected) return;
    client.Poll(ctx.events, frame.dt);
    if (client.Status() == ClientStatus::Disconnecting || client.Status() == ClientStatus::Connecting) return;
    if (m_state == ClientState::Available)
    {
        m_gameWorld.Update(ctx, frame);
        m_gameRenderer.Present(ctx, frameOut);
    }
    else if (m_state == ClientState::RecievedConfig)
    {
        m_gameWorld.Initialize(ctx);
        m_gameRenderer.Initialize(ctx);
        m_state = ClientState::Available;
    }
}
}