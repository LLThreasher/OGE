#pragma once
#include "Engine/IScene.hpp"
#include "Engine/GraphicState.hpp"
#include "Engine/ECS/GameRenderer.hpp"
#include "Engine/ECS/GameWorld.hpp"
#include "Engine/ECS/NetClient.hpp"

namespace OneGame
{
using namespace Engine;

struct ClientArgs
{
    std::string ip = "127.0.0.1";
    uint16_t port = 25567;
    uint32_t timeout = 5000;
};

class DebugClient : public Scene<PresentationContext, const FrameInputData, FrameOutputData>
{
public:
    DebugClient() : m_gameRenderer(m_gameWorld) {}
    virtual void Initialize(PresentationContext& context) override;
    virtual void Enter(PresentationContext& context) override;
    virtual void Update(PresentationContext& context, const FrameInputData& frame, FrameOutputData& frameOut) override;
private:
    ECS::GameWorld m_gameWorld;
    ECS::GameRenderer m_gameRenderer;
    NetClient m_client;
};
} // namespace OneGame
