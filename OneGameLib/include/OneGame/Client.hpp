#pragma once
#include "Engine/IScene.hpp"
#include "Engine/GraphicState.hpp"
#include "Engine/ECS/GameRenderer.hpp"
#include "Engine/ECS/GameWorld.hpp"
#include "Engine/ECS/NetClient.hpp"
#include "Engine/ECS/GameNetSerializer.hpp"

namespace OneGame
{
using namespace Engine;

struct ClientArgs
{
    std::string ip = "127.0.0.1";
    uint16_t port = 25567;
    uint32_t timeout = 5000;
};

enum class ClientState
{
    WaitingConnect,
    WaitingConfig,
    ReceivedConfig,
    Available,
    Disconnected,
};

class DebugClient : public Scene<PresentationContext, const FrameInputData, FrameOutputData>
{
public:
    virtual void Initialize(PresentationContext& context) override;
    virtual void Enter(PresentationContext& context) override;
    virtual void Update(PresentationContext& context, const FrameInputData& frame, FrameOutputData& frameOut) override;
private:
    void onClientConnected(OnClientConnected&);
    void onClientDisconnected(OnClientDisconnected&);
    void onClientConnectionTimeout(OnClientConnectionTimeout&);
    void onPacketReceived(OnClientPacketReceived&);
    
    ECS::GameWorld m_gameWorld;
    ECS::GameUpdateScheduler m_gameUpdater;
    ECS::GameRenderer m_gameRenderer;
    ClientState m_state = ClientState::WaitingConnect;
    GameClient m_client;
};
} // namespace OneGame
