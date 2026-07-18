#include "OneGame/DebugScene.hpp"

#include "Engine/Graphics/PresentationObjects.hpp"
#include "Engine/Graphics/Renderer.hpp"
#include "Engine/Random.hpp"
#include "Engine/StreamingManager.hpp"
#include "Engine/Terrain/BlockManager.hpp"
#include "Engine/Terrain/TerrainService.hpp"
#include "Engine/Terrain/TerrainRenderer.hpp"
#include "Engine/Terrain/TerrainVertexFormat.hpp"
#include "Engine/ECS/IRenderer.hpp"
#include "Engine/ECS/ISubsystem.hpp"

#define LOGGER_NAME "Engine"
#include "Engine/Logger.hpp"

namespace OneGame
{
void DebugScene::Initialize(PresentationContext& context)
{
    using namespace ECS;
    m_gameRenderer.Register<DebugInfoRenderer>();
}

void DebugScene::Enter(PresentationContext& context)
{
    m_gameWorld.Initialize(context);
    m_gameRenderer.Initialize(context);
}

void DebugScene::Exit(PresentationContext& context)
{
}

void DebugScene::Update(PresentationContext& context, const FrameInputData& frame, FrameOutputData& frameOut)
{
    m_gameWorld.Update(context, frame);
    m_gameRenderer.Present(context, frameOut);
}
}  // namespace OneGame::Engine
