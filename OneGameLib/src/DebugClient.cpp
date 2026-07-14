#include "Engine/ECS/GameNetSerializer.hpp"
#include "OneGame/Client.hpp"

namespace OneGame
{
void DebugClient::Initialize(PresentationContext& ctx)
{
    using namespace ECS;
    m_gameWorld.CreateClient();
    m_gameWorld.CreateTerrain();

    m_gameUpdater =
        GameUpdateScheduler::Builder().With<SubsystemUI>().With<SubsystemPlayerInput>().With<SubsystemPlayer>().Build(
            ctx);

    m_gameRenderer = GameRenderer::Builder()
                         .With<DebugInfoRenderer>()
                         .With<Terrain::TerrainRenderer>()
                         .With<UIRenderer>()
                         .With<CameraRenderer>()
                         .With<PlayerInputRenderer>()
                         .Build(ctx);
}

void DebugClient::Enter(PresentationContext& ctx)
{
    ClientArgs _args{};
    auto args = ctx.sceneArgs.try_cast<ClientArgs>();
    if (!args)
    {
        args = &_args;
    }
    auto& client = m_gameWorld.Get().ctx().get<NetClient>();
    client.Initialize();
    client.Connect(args->ip.c_str(), args->port, args->timeout);
    m_client.Initialize(m_gameWorld.Get(), ctx.events);

    ctx.events.sink<OnClientPacketReceived>().connect<&DebugClient::onPacketReceived>(this);
    ctx.events.sink<OnClientConnected>().connect<&DebugClient::onClientConnected>(this);

    m_state = ClientState::WaitingConnect;
}

void DebugClient::onClientDisconnected(OnClientDisconnected&) {}

void DebugClient::onClientConnectionTimeout(OnClientConnectionTimeout&) {}

void DebugClient::onClientConnected(OnClientConnected&) { m_state = ClientState::WaitingConfig; }

void DebugClient::onPacketReceived(OnClientPacketReceived& packet)
{
    Net::Buffer buf{packet.data, packet.length};
    if (m_state == ClientState::WaitingConfig)
    {
        m_client.HandleInitPacket(buf, m_gameWorld.Get());
        m_state = ClientState::ReceivedConfig;
    }
    else if (m_state != ClientState::Disconnected)
    {
        m_client.HandlePacket(buf, m_gameWorld.Get());
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
        m_gameUpdater.Update(m_gameWorld.Get(), ctx, frame);
        m_gameRenderer.Present(m_gameWorld.Get(), ctx, frameOut);
    }
    else if (m_state == ClientState::ReceivedConfig)
    {
        m_gameUpdater.Initialize(m_gameWorld.Get(), ctx);
        m_gameRenderer.Initialize(m_gameWorld.Get(), ctx);
        m_state = ClientState::Available;
    }
}
}  // namespace OneGame